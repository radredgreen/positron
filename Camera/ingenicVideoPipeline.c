/*
  * Threads:
  *
  *     homekit
  *         Description: Handles pairing, Calls event handlers
  * 
  *     gpio (NOT IMPLEMENTED YET)
  *         Description: polls the button
  * 
  *     day_night
  *         Description: polls the brightness and sets the leds and IR filter
  * 
  *     udp input listener (ephemeral) (NOT IMPLEMENTED YET)
  *         Description: listens for speaker data and handles rtcp
  * 
  *     udp output listener (ephemeral) (NOT IMPLEMENTED YET)
  *         Description: listens for rctp data
  * 
  *     video 0 thread (SRTP video stream) (ephemeral) (NOT connected to Homekit YET)
  *         Description: waits for NALs and creates SRTP packets
  * 
  *     video 1 thread ( HKSV ) (ephemeral) (NOT IMPLEMENTED YET)
  *         Description: buffers video data and sends mp4 frames to hub / apple tv
  *         Created By:
  *         Destroyed By:
  *         Pipe Inputs:
  *         Pipe Outputs:
  * 
  *     snapshot thread ( jpeg ) (ephemeral)
  *         Description: buffers video data and sends mp4 frames to hub / apple tv
  *         Created By:
  *         Destroyed By:
  *         Pipe Inputs:
  *         Pipe Outputs:
  * 
  *     audio input (ephemeral) (NOT IMPLEMENTED YET)
  *         Description: waits for microphone data and sends srtp packets
  * 
  *     motion (ephemeral) (NOT connected to Homekit YET)
  *         Description: polls the motion sensor and updates homekit status
  * 
*/


/*
  * framesource.0 ------------------------> encoder group 0 -----> encoder channel 0 (global encChn 0) ( H264 SRTP Videostream )
  * framesource.1 -----> ivs group 0 -----> encoder group 1 -----> encoder channel 0 (global encChn 1) ( H264 HKSV DVR )
  *                                                           +--> encoder channel 1 (global encChn 2) ( JPEG ( Scale ) )
*/



#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_ivs.h>
#include <imp/imp_ivs_move.h>
#include <imp/imp_isp.h>

#include <imp/imp_osd.h>

#include "pwm.h"

#include <HAP.h>
#include <HAP+Internal.h>

static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "videoPipeline" };

#define TAG "videoPipeline"

#define SENSOR_FRAME_RATE_NUM			25
#define SENSOR_FRAME_RATE_DEN			1
#define SENSOR_FRAME_RATE_NUM_NIGHT		10

#define SENSOR_NAME				"gc2053"
#define SENSOR_CUBS_TYPE        TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDR			0x37
#define SENSOR_WIDTH			1920
#define SENSOR_HEIGHT			1080
#define CROP_EN					0


#define IRCUT_EN_GPIO 52 
#define IRCUT_DIS_GPIO 53

//for gc2053
#define EXP_NIGHT_THRESHOLD 30000 
#define EXP_DAY_THRESHOLD 5900 
#define EXP_IR_THRESHOLD 30000

#define STRINGIFY__(a) #a
#define STRINGIFY(a) STRINGIFY__(a)

// Enables the IR cut filter, which blocks out IR light for day time.
// enable=1 for day time, enable=0 for night.
// credit: https://raw.githubusercontent.com/geekman/t20-rtspd/master/imp-common.c
int sample_set_IRCUT(int enable)
{

	int fd, fd1, fd2;
	char on[4], off[4];

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if(fd < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/export error !");
		return -1;
	}

	write(fd, STRINGIFY(IRCUT_DIS_GPIO), 2);
	write(fd, STRINGIFY(IRCUT_EN_GPIO), 2);

	close(fd);

	fd1 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/direction", O_RDWR);
	if(fd1 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/direction error !");
		return -1;
	}

	fd2 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_EN_GPIO) "/direction", O_RDWR);
	if(fd2 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_EN_GPIO) "/direction error !");
		return -1;
	}

	write(fd1, "out", 3);
	write(fd2, "out", 3);

	close(fd1);
	close(fd2);

	fd1 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/active_low", O_RDWR);
	if(fd1 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/active_low error !");
		return -1;
	}

	fd2 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/value", O_RDWR);
	if(fd2 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_EN_GPIO) "/active_low error !");
		return -1;
	}

	write(fd1, "0", 1);
	write(fd2, "0", 1);

	close(fd1);
	close(fd2);

	fd1 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/value", O_RDWR);
	if(fd1 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_DIS_GPIO) "/value error !");
		return -1;
	}

	fd2 = open("/sys/class/gpio/gpio" STRINGIFY(IRCUT_EN_GPIO) "/value", O_RDWR);
	if(fd2 < 0) {
		HAPLogError(&logObject, "open /sys/class/gpio/gpio" STRINGIFY(IRCUT_EN_GPIO) "/value error !");
		return -1;
	}

	if (enable) {
		write(fd2, "1", 1);
	} else {
		write(fd1, "1", 1);
	}

	usleep(500*1000);
	write(fd1, "0", 1);
	write(fd2, "0", 1);

	close(fd1);
	close(fd2);
	return 0;

}


#define EXP_LENGTH (8 /*sec*/ * 24 /*cycles*/)
#define NUM_EXP_VALUES (EXP_LENGTH / 8)


char *get_curr_timestr(char *buf) {
	time_t t;
	struct tm *tminfo;

	time(&t);
	tminfo = localtime(&t);
	sprintf(buf, "%02d:%02d:%02d", tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec);
	return buf;
}
static int  g_soft_ps_running = 1;
//TODO: make the IRLED controllable by homekit switch
static int ir_illumination = 0;		// use IR LEDs when dark
static int force_color = 1;			// use color mode, even at night
static int flip_image = 0;			// flip 180deg for ceiling mounts

// credit: https://raw.githubusercontent.com/geekman/t20-rtspd/master/imp-common.c
void *sample_soft_photosensitive_ctrl(void *p)
{
	int evDebugCount = 0;
	int hysteresisCount = 5;
	char tmstr[16];
	int avgExp;
	int expCount = 0;
	int expValues[NUM_EXP_VALUES];
	int expPos = 0;
	IMPISPRunningMode pmode = IMPISP_RUNNING_MODE_BUTT;
	int ir_leds_active = 0;
	int r;

	// initialize PWM
	struct pwm_ioctl_t pwm_cfg = {
		.channel = 0,
		.period  = 1000000,
		.duty    = 0,
		.polarity = 1,\
	};

	if (pwm_init() < 0) {
		HAPLogError(&logObject, "cant pwm_init(): errno %d", errno);
	} else if ((r = pwm_config(&pwm_cfg)) < 0) {
		HAPLogError(&logObject, "cant config pwm: %d, errno %d", r, errno);
	} else if ((r = pwm_enable(pwm_cfg.channel)) < 0) {
		HAPLogError(&logObject, "cant enable pwm channel: %d, errno %d", r, errno);
	}

	// make sure it's in an "uninitialized" state
	expValues[NUM_EXP_VALUES - 1] = 0;

	sleep(2);	// wait a few for ISP to settle

	while (g_soft_ps_running) {
		IMPISPEVAttr expAttr;
		int ret = IMP_ISP_Tuning_GetEVAttr(&expAttr);
		if (ret == 0) {
			if (evDebugCount > 0) {
				HAPLogDebug(&logObject, "EV attr: exp %d aGain %d dGain %d\n",
						expAttr.ev, expAttr.again, expAttr.dgain);
				evDebugCount--;
			}
		} else
			return NULL;

		if (expCount == 0) {
			avgExp = expAttr.ev;
		} else {
			// exponential moving average
			avgExp -= avgExp / expCount;
			avgExp += expAttr.ev / expCount;
		}

		if (expCount < 3) expCount++;

		// keep a longer running average exposure
		if (expPos % 8 == 0)
			expValues[expPos / 8] = avgExp;
		if (++expPos >= EXP_LENGTH)
			expPos = 0;

		// calculate long average exposure value
		int longExpValue = 0;
		if (expValues[NUM_EXP_VALUES - 1] == 0) {
			longExpValue = expValues[0];
		} else {
			int stopPos = (expPos / 8) - 8;	// ignore recent 8 cycles
			if (stopPos < 0) stopPos += NUM_EXP_VALUES;

			int i, j;
			for (i = 1, j = expPos / 8;
					j < stopPos;
					i++, j = (j + 1) % NUM_EXP_VALUES)
				longExpValue += (expValues[j] - longExpValue) / i;
		}
		HAPLogDebug(&logObject,"avgExp: %d, expPos: %d, longExpValue %d\n ", avgExp, expPos, longExpValue);
		// if the hysteresisCount is active, we skip any changes below
		if (hysteresisCount > 0) {
			hysteresisCount--;
			goto end;
		}

		///////////////////////////////////////////////////////////////


		if (avgExp > EXP_NIGHT_THRESHOLD) {
			if (pmode != IMPISP_RUNNING_MODE_NIGHT) {
				pmode = IMPISP_RUNNING_MODE_NIGHT;
				HAPLogDebug(&logObject, "[%s] avg exp is %d. switching to night mode\n",
						get_curr_timestr((char *) &tmstr), avgExp);
				evDebugCount = 10; // start logging 10s of EV data
				hysteresisCount = 5;

				if (! force_color) {
					IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_NIGHT);
					IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM_NIGHT, SENSOR_FRAME_RATE_DEN);
				} else {
					HAPLogDebug(&logObject, "[%s] force_color is on\n",
						get_curr_timestr((char *) &tmstr));
				}
				sample_set_IRCUT(0);
			}
		} else if (avgExp < EXP_DAY_THRESHOLD) {
			if (pmode != IMPISP_RUNNING_MODE_DAY) {
				pmode = IMPISP_RUNNING_MODE_DAY;
				HAPLogDebug(&logObject, "[%s] avg exp is %d. switching to day mode\n",
						get_curr_timestr((char *) &tmstr), avgExp);
				evDebugCount = 10; // start logging 10s of EV data
				hysteresisCount = 5;

				if (! force_color) {
					IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY);
					IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN);
				}
				sample_set_IRCUT(1);
			}
		}

		// control IR LEDs
		if (avgExp > EXP_IR_THRESHOLD) {
			// only log for first time
			if (! ir_leds_active) {
				HAPLogDebug(&logObject, "[%s] avg exp is %d. turning on IR LEDs\n",
						get_curr_timestr((char *) &tmstr), avgExp);
				evDebugCount = 10; // start logging 10s of EV data
				hysteresisCount = 5;
			}

			// it's ok to go higher, but not lower than long-running average
			// this prevents sudden bright bursts of light from dimming IR LEDs
			int ev = (avgExp > longExpValue) ? avgExp : longExpValue;

			// map 3 to 13mil => 1 to 1mil for PWM
			int level = (ev - EXP_IR_THRESHOLD) * 3;

			// cap to maximum
			if (level > pwm_cfg.period)
				level = pwm_cfg.period;

			if (ir_illumination) pwm_set_duty(pwm_cfg.channel, level);
			ir_leds_active = 1;
		} else if (ir_leds_active) {
			HAPLogDebug(&logObject, "[%s] avg exp is %d. turning off IR LEDs\n",
						get_curr_timestr((char *) &tmstr), avgExp);
			evDebugCount = 10; // start logging 10s of EV data
			hysteresisCount = 5;

			if (ir_illumination) pwm_set_duty(pwm_cfg.channel, 0);
			ir_leds_active = 0;
		}

end:
		sleep(1);
	}

	return NULL;
}



static void *sample_ivs_move_get_result_process(void *arg)
{
	int i = 0, ret = 0;
	int chn_num = (int)arg;
	IMP_IVS_MoveOutput *result = NULL;

	for (i = 0; true; i++) {
		ret = IMP_IVS_PollingResult(chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
		if (ret < 0) {
			HAPLogError(&logObject, "IMP_IVS_PollingResult(%d, %d) failed\n", chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
			return (void *)-1;
		}
		ret = IMP_IVS_GetResult(chn_num, (void **)&result);
		if (ret < 0) {
			HAPLogError(&logObject, "IMP_IVS_GetResult(%d) failed\n", chn_num);
			return (void *)-1;
		}
		HAPLogDebug(&logObject,"frame[%d], result->retRoi(%d,%d,%d,%d)\n", i, result->retRoi[0], result->retRoi[1], result->retRoi[2], result->retRoi[3]);

		ret = IMP_IVS_ReleaseResult(chn_num, (void *)result);
		if (ret < 0) {
			HAPLogError(&logObject, "IMP_IVS_ReleaseResult(%d) failed\n", chn_num);
			return (void *)-1;
		}
#if 0
		if (i % 20 == 0) {
			ret = sample_ivs_set_sense(chn_num, i % 5);
			if (ret < 0) {
				HAPLogError(&logObject, "sample_ivs_set_sense(%d, %d) failed\n", chn_num, i % 5);
				return (void *)-1;
			}
		}
#endif
	}

	return (void *)0;
}

static int frmrate_sp[3] = { 0 };
static int statime_sp[3] = { 0 };
static int bitrate_sp[3] = { 0 };

static void *get_srtp_video_stream(void *args)
{
  int val, i, chnNum, ret;
  char stream_path[64];
  IMPEncoderEncType encType;
  int stream_fd = -1, totalSaveCnt = 0;

  val = (int)args;
  chnNum = val & 0xffff;
  encType = (val >> 16) & 0xffff;

  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed\n", chnNum);
    return ((void *)-1);
  }

  //for (i = 0; i < totalSaveCnt; i++) {
  // //TODO: in the future, check the mutex for changes to the stream here
  while(1){
    ret = IMP_Encoder_PollingStream(chnNum, 1000);
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout\n", chnNum);
      continue;
    }

    IMPEncoderStream stream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &stream, 1);
	if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed\n", chnNum);
      return ((void *)-1);
    }

#if 1
	int i, len = 0;
    for (i = 0; i < stream.packCount; i++) {
      len += stream.pack[i].length;
    }
    bitrate_sp[chnNum] += len;
    frmrate_sp[chnNum]++;

    int64_t now = IMP_System_GetTimeStamp() / 1000;
    if(((int)(now - statime_sp[chnNum]) / 1000) >= 2){ // report bit rate every 2 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);

      HAPLogInfo(&logObject,"streamNum[%d]:FPS: %0.2f,Bitrate: %0.2f(kbps)\n", chnNum, fps, kbr);
      
      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }
#endif
//TODO: for now just dump the data
#if 0
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed\n", chnNum);
      return ((void *)-1);
    }

    if (encType == IMP_ENC_TYPE_JPEG) {
      ret = save_stream_by_name(stream_path, i, &stream);
      if (ret < 0) {
        return ((void *)ret);
      }
    }
    else {
      ret = save_stream(stream_fd, &stream);
      if (ret < 0) {
        close(stream_fd);
        return ((void *)ret);
      }
    }
#endif
    IMP_Encoder_ReleaseStream(chnNum, &stream);
  }

  ret = IMP_Encoder_StopRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed\n", chnNum);
    return ((void *)-1);
  }

  return ((void *)0);
}


pthread_mutex_t snapshot_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *get_jpeg_stream(void *args)
{
  int val, i, chnNum, ret;
  char stream_path[64];
  IMPEncoderEncType encType;
  int stream_fd = -1, totalSaveCnt = 0;

  val = (int)args;
  chnNum = val & 0xffff;
  encType = (val >> 16) & 0xffff;

  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed\n", chnNum);
    return ((void *)-1);
  }

  //for (i = 0; i < totalSaveCnt; i++) {
  // //TODO: in the future, check the mutex for changes to the stream here
  while(1){
    ret = IMP_Encoder_PollingStream(chnNum, 5000);
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout\n", chnNum);
      continue;
    }

    IMPEncoderStream stream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &stream, 1);
	if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed\n", chnNum);
      return ((void *)-1);
    }

#if 1
	int i, len = 0;
    for (i = 0; i < stream.packCount; i++) {
      len += stream.pack[i].length;
    }
    bitrate_sp[chnNum] += len;
    frmrate_sp[chnNum]++;

    int64_t now = IMP_System_GetTimeStamp() / 1000;
    if(((int)(now - statime_sp[chnNum]) / 1000) >= 2){ // report bit rate every 2 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);

      HAPLogDebug(&logObject,"streamNum[%d]:FPS: %0.2f,Bitrate: %0.2f(kbps)\n", chnNum, fps, kbr);
      //fflush(stdout);

      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }
#endif
	char * snap_path = "/tmp/snap.jpg";

	HAPLogInfo(&logObject, "Locking snapshot mutex");
	if ( pthread_mutex_lock(&snapshot_mutex) != 0){
		HAPLogError(&logObject, "Locking snapshot mutex failed: %s\n", strerror(errno));
	}

	HAPLogInfo(&logObject, "Open Snap file %s ", snap_path);
	int snap_fd = open(snap_path, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (snap_fd < 0) {
		HAPLogError(&logObject, "failed: %s\n", strerror(errno));
	}
	else{
		HAPLogInfo(&logObject, "OK\n");

		int ret, nr_pack = stream.packCount;

		//HAPLogDebug(&logObject, "----------packCount=%d, stream.seq=%u start----------\n", stream.packCount, stream.seq);
		for (i = 0; i < nr_pack; i++) {
		//HAPLogDebug(&logObject, "[%d]:%10u,%10lld,%10u,%10u,%10u\n", i, stream.pack[i].length, stream.pack[i].timestamp, stream.pack[i].frameEnd, *((uint32_t *)(&stream.pack[i].nalType)), stream.pack[i].sliceType);
			IMPEncoderPack *pack = &stream.pack[i];
			if(pack->length){
				uint32_t remSize = stream.streamSize - pack->offset;
				if(remSize < pack->length){
					ret = write(snap_fd, (void *)(stream.virAddr + pack->offset), remSize);
					if (ret != remSize) {
						HAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].remSize(%d) error:%s\n", ret, i, remSize, strerror(errno));
						// return -1;
					}
					ret = write(snap_fd, (void *)stream.virAddr, pack->length - remSize);
					if (ret != (pack->length - remSize)) {
						HAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].(length-remSize)(%d) error:%s\n", ret, i, (pack->length - remSize), strerror(errno));
						// return -1;
					}
				}else {
					ret = write(snap_fd, (void *)(stream.virAddr + pack->offset), pack->length);
					if (ret != pack->length) {
						HAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].length(%d) error:%s\n", ret, i, pack->length, strerror(errno));
						// return -1;
					}
				}
			}
		}
		close(snap_fd);

	//HAPLogDebug(&logObject, "----------packCount=%d, stream.seq=%u end----------\n", stream.packCount, stream.seq);
	}

	HAPLogInfo(&logObject, "Unlocking snapshot mutex");
	if ( pthread_mutex_unlock(&snapshot_mutex) != 0){
		HAPLogError(&logObject, "Unocking snapshot mutex failed: %s\n", strerror(errno));
	}

    IMP_Encoder_ReleaseStream(chnNum, &stream);
  }

  ret = IMP_Encoder_StopRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed\n", chnNum);
    return ((void *)-1);
  }

  return ((void *)0);
}


extern int IMP_OSD_SetPoolSize(int size);
//extern int IMP_Encoder_SetPoolSize(int size);


int initVideoPipeline(){

	int ret = 0;
	IMPSensorInfo sensor_info;

	IMP_OSD_SetPoolSize(512*1024);
	IMP_Log_Set_Option(IMP_LOG_OP_ALL);

	HAPLogInfo(&logObject, "initVideoPipeline start\n");


	ret = IMP_ISP_Open();
	if(ret < 0){
		HAPLogError(&logObject,"failed to open ISP\n");
		return -1;
	}
	
	HAPLogDebug(&logObject, "IMP_System_Init Finish. \n");

	memset(&sensor_info, 0, sizeof(IMPSensorInfo));
	memcpy(sensor_info.name, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.cbus_type = SENSOR_CUBS_TYPE;
	memcpy(sensor_info.i2c.type, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.i2c.addr = SENSOR_I2C_ADDR;

	HAPLogDebug(&logObject, "IMP_ISP_AddSensor start\n");
	ret = IMP_ISP_AddSensor(&sensor_info);
	if(ret < 0){
		HAPLogError(&logObject,"failed to AddSensor\n");
		return -1;
	}

	HAPLogDebug(&logObject, "IMP_ISP_EnableSensor start\n");
	ret = IMP_ISP_EnableSensor();
	if(ret < 0){
		HAPLogError(&logObject,"failed to EnableSensor\n");
		return -1;
	}

	HAPLogDebug(&logObject, "IMP_System_Init start\n");
	ret = IMP_System_Init();
	if(ret < 0){
		HAPLogError(&logObject,"IMP_System_Init failed\n");
		return -1;
	}


	HAPLogDebug(&logObject, "IMP_ISP_EnableTuning start\n");
#if 1

	/* enable turning, to debug graphics */
	ret = IMP_ISP_EnableTuning();
	if(ret < 0){
		HAPLogError(&logObject,"IMP_ISP_EnableTuning failed\n");
		return -1;
	}
#endif
#if 1
    IMP_ISP_Tuning_SetContrast(128);
    IMP_ISP_Tuning_SetSharpness(128);
    IMP_ISP_Tuning_SetSaturation(128);
    IMP_ISP_Tuning_SetBrightness(128);
#endif
#if 1
	HAPLogDebug(&logObject, "IMP_ISP_Tuning_SetISPRunningMode start\n");
    ret = IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY);
    if (ret < 0){
        HAPLogError(&logObject,"failed to set running mode\n");
        return -1;
    }
#endif
#if 1
    ret = IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN);
    if (ret < 0){
        HAPLogError(&logObject,"failed to set sensor fps\n");
        return -1;
    }
#endif
	HAPLogDebug(&logObject, "ImpSystemInit success\n");


	IMPFSChnAttr fs0_chn_attr = {
				.pixFmt = PIX_FMT_NV12,
				.outFrmRateNum = SENSOR_FRAME_RATE_NUM,
				.outFrmRateDen = SENSOR_FRAME_RATE_DEN,
				.nrVBs = 2,
				.type = FS_PHY_CHANNEL,

				.crop.enable = CROP_EN,
				.crop.top = 0,
				.crop.left = 0,
				.crop.width = SENSOR_WIDTH,
				.crop.height = SENSOR_HEIGHT,

				.scaler.enable = 0,
				.scaler.outwidth = SENSOR_WIDTH,
				.scaler.outheight = SENSOR_HEIGHT,

				.picWidth = SENSOR_WIDTH,
				.picHeight = SENSOR_HEIGHT,
			};

	HAPLogDebug(&logObject, "Setting up Framesource 0 \n");
	ret = IMP_FrameSource_CreateChn(0 /*index*/, &fs0_chn_attr);
	if(ret < 0){
		HAPLogError(&logObject,"IMP_FrameSource_CreateChn(chn%d) error !\n", 0);
		return -1;
	}

	ret = IMP_FrameSource_SetChnAttr(0 /*index*/, &fs0_chn_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_SetChnAttr(chn%d) error !\n",  0);
		return -1;
	}

#if 1
	IMPFSChnAttr fs1_chn_attr = {
				.pixFmt = PIX_FMT_NV12,
				.outFrmRateNum = SENSOR_FRAME_RATE_NUM,
				.outFrmRateDen = SENSOR_FRAME_RATE_DEN,
				.nrVBs = 2,
				.type = FS_PHY_CHANNEL,

				.crop.enable = CROP_EN,
				.crop.top = 0,
				.crop.left = 0,
				.crop.width = SENSOR_WIDTH,
				.crop.height = SENSOR_HEIGHT,

				.scaler.enable = 0,
				.scaler.outwidth = SENSOR_WIDTH,
				.scaler.outheight = SENSOR_HEIGHT,

				.picWidth = SENSOR_WIDTH,
				.picHeight = SENSOR_HEIGHT,
			};
			
	HAPLogDebug(&logObject, "Setting up Framesource 1 \n");
	ret = IMP_FrameSource_CreateChn(1 /*index*/, &fs1_chn_attr);
	if(ret < 0){
		HAPLogError(&logObject,"IMP_FrameSource_CreateChn(chn%d) error !\n", 1);
		return -1;
	}

	ret = IMP_FrameSource_SetChnAttr(1 /*index*/, &fs1_chn_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_SetChnAttr(chn%d) error !\n",  1);
		return -1;
	}
#endif
	HAPLogDebug(&logObject, "Setting up encoder group 0 \n");

	ret = IMP_Encoder_CreateGroup( 0 /* index*/ );
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateGroup(%d) error !\n", 0);
		return -1;
	}


	IMPEncoderChnAttr enc0_channel0_attr;
	//TODO: experiment with different encoder rc modes
 	static const IMPEncoderRcMode S_RC_METHOD = IMP_ENC_RC_MODE_CBR; //IMP_ENC_RC_MODE_CAPPED_VBR; //IMP_ENC_RC_MODE_FIXQP; //IMP_ENC_RC_MODE_CAPPED_QUALITY;

	ret = IMP_Encoder_SetDefaultParam(&enc0_channel0_attr, IMP_ENC_PROFILE_HEVC_MAIN, S_RC_METHOD,
			SENSOR_WIDTH, SENSOR_HEIGHT,
			SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN,
			SENSOR_FRAME_RATE_NUM * 2 / SENSOR_FRAME_RATE_DEN, // GOP length (2 secc)
			2, // uMaxSameSenceCnt ??
			(S_RC_METHOD == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1, // iInitialQP
			1000); // uTargetBitRate
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !\n", 0);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 0, &enc0_channel0_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !\n", 0);
		return -1;
	}

	ret = IMP_Encoder_RegisterChn(0 /* encGroup */, 0 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d\n", 0, 0, ret);
		return -1;
	}


	HAPLogDebug(&logObject, "Setting up encoder group 1: h264 \n");

#if 1
	ret = IMP_Encoder_CreateGroup( 1 /* index*/ );
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateGroup(%d) error !\n", 1);
		return -1;
	}

	IMPEncoderChnAttr enc1_channel0_attr;

	ret = IMP_Encoder_SetDefaultParam(&enc1_channel0_attr, IMP_ENC_PROFILE_HEVC_MAIN, S_RC_METHOD,
			SENSOR_WIDTH, SENSOR_HEIGHT,
			SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN,
			SENSOR_FRAME_RATE_NUM * 2 / SENSOR_FRAME_RATE_DEN, // GOP length (2 sec)
			2, // uMaxSameSenceCnt ??
			(S_RC_METHOD == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1, // iInitialQP
			1000 ); // uTargetBitRate
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !\n", 1);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 1, &enc1_channel0_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !\n", 1);
		return -1;
	}

 	ret = IMP_Encoder_RegisterChn( 1 /* encGroup */, 1 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d\n", 1, 1, ret);
		return -1;
	}


	HAPLogDebug(&logObject, "Setting up encoder group 2: jpeg \n");

	IMPEncoderChnAttr enc1_channel1_attr;

	#define JPEG_FRAME_RATE_NUM 1
	#define JPEG_FRAME_RATE_DEN 1

	ret = IMP_Encoder_SetDefaultParam(&enc1_channel1_attr, IMP_ENC_PROFILE_JPEG, IMP_ENC_RC_MODE_FIXQP,
			640, 480, // SENSOR_WIDTH, SENSOR_HEIGHT,
			JPEG_FRAME_RATE_NUM, JPEG_FRAME_RATE_DEN,
			0, 0, 25, 0);

	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !\n", 2);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 2, &enc1_channel1_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !\n", 2);
		return -1;
	}

	ret = IMP_Encoder_RegisterChn( 1 /* encGroup */, 2 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d\n", 1, 2, ret);
		return -1;
	}

	HAPLogDebug(&logObject, "Setting up motion detection \n");

	ret = IMP_IVS_CreateGroup(0);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_IVS_CreateGroup(%d) failed\n", 0);
		return -1;
	}
#endif

	HAPLogDebug(&logObject, "Binding video path for SRTP \n");

	IMPCell	framesource0_chn =	{ DEV_ID_FS, 0, 0};
	IMPCell imp_encoder0 = { DEV_ID_ENC, 0, 0};
	
	ret = IMP_System_Bind(&framesource0_chn, &imp_encoder0);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind FrameSource channel.0 output.0 and encoder0 failed\n");
		return -1;
	}
#if 1


	IMPCell	framesource1_chn =	{ DEV_ID_FS, 1, 0};
	IMPCell ivs_cell = {DEV_ID_IVS, 0, 0};

	HAPLogDebug(&logObject, "Binding video path for motion \n");
	ret = IMP_System_Bind(&framesource1_chn, &ivs_cell);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind FrameSource channel.1 output.0 and ivs0 failed\n");
		return -1;
	}
	IMPCell imp_encoder1 = { DEV_ID_ENC, 1, 0};
	HAPLogDebug(&logObject, "Binding video path for HKSV\n");
	ret = IMP_System_Bind(&ivs_cell, &imp_encoder1);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind ivs0 channel%d and encoder1 failed\n",0);
		return -1;
	}

	// Do I need to bind the jpeg encoder??
	// I don't think so, sample-Encoder-h264-jpeg.c line 79 only binds the h264+jpeg encoding group, not the jpeg encoder
	// IMPCell imp_encoder2 = { DEV_ID_ENC, 2, 0};
	// HAPLogDebug(&logObject, "Binding video path for JPEG \n");
	// ret = IMP_System_Bind(&ivs_cell, &imp_encoder2);
	// if (ret < 0) {
	// 	HAPLogError(&logObject,"Bind ivs0 channel%d and encoder2 failed\n",0);
	// 	return -1;
	// }

#endif
	HAPLogDebug(&logObject, "Starting Frame Sources \n");

	ret = IMP_FrameSource_EnableChn(0);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_EnableChn(%d) error: %d\n", 0, ret);
		return -1;
	}
#if 1

	ret = IMP_FrameSource_EnableChn(1);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_EnableChn(%d) error: %d\n", 1, ret);
		return -1;
	}
#endif

	HAPLogDebug(&logObject, "Starting motion sensing \n");

	IMP_IVS_MoveParam ivs_param;
	IMPIVSInterface *ivs_interface = NULL;
	int i = 0, j = 0;

	memset(&ivs_param, 0, sizeof(IMP_IVS_MoveParam));
	ivs_param.skipFrameCnt = 5;
	ivs_param.frameInfo.width = SENSOR_WIDTH;
	ivs_param.frameInfo.height = SENSOR_HEIGHT;
	ivs_param.roiRectCnt = 4;

	//TODO: something to do with sensitivity?
	for(i=0; i<ivs_param.roiRectCnt; i++){
	  ivs_param.sense[i] = 4;
	}

	/* HAPLogInfo(&logObject,"ivs_param.sense=%d, ivs_param.skipFrameCnt=%d, ivs_param.frameInfo.width=%d, ivs_param.frameInfo.height=%d\n", ivs_param.sense, ivs_param.skipFrameCnt, ivs_param.frameInfo.width, ivs_param.frameInfo.height); */
	//TODO: setup regions of interest
	for (j = 0; j < 2; j++) {
		for (i = 0; i < 2; i++) {
		  if((i==0)&&(j==0)){
			ivs_param.roiRect[j * 2 + i].p0.x = i * ivs_param.frameInfo.width /* / 2 */;
			ivs_param.roiRect[j * 2 + i].p0.y = j * ivs_param.frameInfo.height /* / 2 */;
			ivs_param.roiRect[j * 2 + i].p1.x = (i + 1) * ivs_param.frameInfo.width /* / 2 */ - 1;
			ivs_param.roiRect[j * 2 + i].p1.y = (j + 1) * ivs_param.frameInfo.height /* / 2 */ - 1;
			HAPLogDebug(&logObject,"(%d,%d) = ((%d,%d)-(%d,%d))\n", i, j, ivs_param.roiRect[j * 2 + i].p0.x, ivs_param.roiRect[j * 2 + i].p0.y,ivs_param.roiRect[j * 2 + i].p1.x, ivs_param.roiRect[j * 2 + i].p1.y);
		  }
		  else
		    {
		      	ivs_param.roiRect[j * 2 + i].p0.x = 0;//ivs_param.roiRect[0].p0.x;
			ivs_param.roiRect[j * 2 + i].p0.y = 0;//ivs_param.roiRect[0].p0.y;
			ivs_param.roiRect[j * 2 + i].p1.x = 0;//ivs_param.roiRect[0].p1.x;;
			ivs_param.roiRect[j * 2 + i].p1.y = 0;//ivs_param.roiRect[0].p1.y;;
			HAPLogDebug(&logObject,"(%d,%d) = ((%d,%d)-(%d,%d))\n", i, j, ivs_param.roiRect[j * 2 + i].p0.x, ivs_param.roiRect[j * 2 + i].p0.y,ivs_param.roiRect[j * 2 + i].p1.x, ivs_param.roiRect[j * 2 + i].p1.y);
		    }
		}
	}
	ivs_interface = IMP_IVS_CreateMoveInterface(&ivs_param);
	if (ivs_interface == NULL) {
		HAPLogError(&logObject, "IMP_IVS_CreateGroup(%d) failed\n", /*grp_num*/ 0);
		return -1;
	}

	ret = IMP_IVS_CreateChn(/*chn_num*/ 2, ivs_interface);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_CreateChn(%d) failed\n", /*chn_num*/ 2);
		return -1;
	}

	ret = IMP_IVS_RegisterChn(/*grp_num*/ 0, /*chn_num*/ 2);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_RegisterChn(%d, %d) failed\n", /*grp_num*/ 0, /*chn_num*/ 2);
		return -1;
	}

	ret = IMP_IVS_StartRecvPic(/*chn_num*/ 2);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_StartRecvPic(%d) failed\n", /*chn_num*/ 2);
		return -1;
	}
	
	pthread_t ivs_tid;

	HAPLogDebug(&logObject, "Starting motion sensing thread \n");
	if (pthread_create(&ivs_tid, NULL, sample_ivs_move_get_result_process, (void *) /*chn_num*/ 2) < 0) {
		HAPLogError(&logObject, "create sample_ivs_move_get_result_process failed\n");
		return -1;
	}

	int arg;
	pthread_t srtp_tid;

	HAPLogDebug(&logObject, "Starting srtp capture thread \n");
	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 0));
	ret = pthread_create(&srtp_tid, NULL, get_srtp_video_stream, (void *)arg);
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed\n", (/*chn_num*/ 0 ));
	}

	pthread_t hksv_tid;

	HAPLogDebug(&logObject, "Starting hksv capture thread \n");
	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 1));
	// TODO: in the future, change this to a hskv specific processing thread
	ret = pthread_create(&hksv_tid, NULL, get_srtp_video_stream, (void *)arg);
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed\n", (/*chn_num*/ 1 ));
	}

	pthread_t jpeg_tid;

	HAPLogDebug(&logObject, "Starting jpeg capture thread \n");
	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 2));
	// TODO: in the future, change this to a jpeg specific processing thread
	ret = pthread_create(&jpeg_tid, NULL, get_jpeg_stream, (void *)arg);
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed\n", (/*chn_num*/ 2 ));
	}


	// start thread for activating night mode & IR cut filter
	pthread_t thread_info;
	pthread_create(&thread_info, NULL, sample_soft_photosensitive_ctrl, NULL);
	if (ret < 0) {
		HAPLogError(&logObject, "Failed to start thread for activating night mode & IR cut filter");
	}

	//TODO: join these threads at somepoint




}
