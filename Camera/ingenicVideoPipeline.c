/* 
 * This file is part of the positron distribution (https://github.com/radredgreen/positron).
 * Copyright (c) 2024 RadRedGreen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


/*
  * Threads:
  *
  *     homekit
  *         Description: Handles pairing, Calls event handlers, gets snapshots
  * 
  *     gpio (NOT IMPLEMENTED YET)
  *         Description: monitors the button and does what?
  * 
  *     exposure monitoring
  *         Description: polls the brightness and sets the leds and IR filter
  * 
  *     audio feeback (ephemeral) 
  *         Description: listens for speaker data, pushes audio to ring buffer and handles rtcp
  * 
  *     speaker playback (ephemeral)
  *         Description: waits for ring buffer and pushes audio data
  * 
  *     video feedback (ephemeral) 
  *         Description: listens for video rctp data
  * 
  *     video 0 thread (SRTP video stream) (ephemeral) 
  *         Description: waits for NALs and creates SRTP packets
  * 
  *     video 1 thread ( HKSV ) (ephemeral) 
  *         Description: buffers video data and sends mp4 frames to hub / apple tv
  * 
  *     snapshot thread ( jpeg ) (ephemeral)
  *         Description: buffers video data and sends mp4 frames to hub / apple tv
  * 
  *     audio input (ephemeral) 
  *         Description: waits for microphone data and sends srtp packets
  * 
  *     motion (ephemeral) 
  *         Description: polls the motion sensor and updates homekit status
  * 
*/


/*	
  * Video pipeline configuration:
  *
  * framesource.1 ------------------------> encoder group 0 -----> encoder channel 0 (global encChn 0) ( H264 SRTP Videostream )
  * framesource.0 -----> ivs group 0 -----> encoder group 1 -----> encoder channel 0 (global encChn 1) ( H264 HKSV DVR )
  *                 +--> exposure monitoring                  +--> encoder channel 1 (global encChn 2) ( JPEG ( Scale ) )
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
#include <pthread.h>
#include <sys/prctl.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_ivs.h>
#include <imp/imp_ivs_move.h>
#include <imp/imp_isp.h>

#include <imp/imp_osd.h>

#include "ingenicPwm.h"

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
#define EXP_DAY_THRESHOLD 600 //5900 
#define EXP_IR_OFF_THRESHOLD 600
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
    prctl(PR_SET_NAME, "set_ircut");

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
static int force_color = 0;			// use color mode, even at night
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
	int r, ret;

    prctl(PR_SET_NAME, "photosense");

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

	//temporarily here since it isn't working in the init pipeline
	// flip the image 180deg if requested
	if (flip_image) {
		ret = IMP_ISP_Tuning_SetISPHflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		if (ret < 0){
			HAPLogError(&logObject,"failed to set horiz flip\n");
		}
		IMPISPTuningOpsMode pmode;
		ret = IMP_ISP_Tuning_GetISPHflip(&pmode);
		if (ret < 0){
			HAPLogError(&logObject,"failed to get horiz flip\n");
		}
		if (pmode != IMPISP_TUNING_OPS_MODE_ENABLE){
			HAPLogError(&logObject,"tried to horiz flip, but imp didn't take the change\n");

		}

		ret = IMP_ISP_Tuning_SetISPVflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		if (ret < 0){
			HAPLogError(&logObject,"failed to set vert flip\n");
		}
		ret = IMP_ISP_Tuning_GetISPVflip(&pmode);
		if (ret < 0){
			HAPLogError(&logObject,"failed to get very flip\n");
		}
		if (pmode != IMPISP_TUNING_OPS_MODE_ENABLE){
			HAPLogError(&logObject,"tried to vert flip, but imp didn't take the change\n");

		}
	}


		IMPISPEVAttr expAttr;
		int ret = IMP_ISP_Tuning_GetEVAttr(&expAttr);
		if (ret == 0) {
			if (evDebugCount > 0) {
				HAPLogDebug(&logObject, "EV attr: exp %d aGain %d dGain %d",
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
		//HAPLogDebug(&logObject,"avgExp: %d, expPos: %d, longExpValue %d", avgExp, expPos, longExpValue);
		// if the hysteresisCount is active, we skip any changes below
		if (hysteresisCount > 0) {
			hysteresisCount--;
			goto end;
		}

		///////////////////////////////////////////////////////////////


		if (avgExp > EXP_NIGHT_THRESHOLD) {
			if (pmode != IMPISP_RUNNING_MODE_NIGHT) {
				pmode = IMPISP_RUNNING_MODE_NIGHT;
				HAPLogDebug(&logObject, "[%s] avg exp is %d. switching to night mode",
						get_curr_timestr((char *) &tmstr), avgExp);
				evDebugCount = 10; // start logging 10s of EV data
				hysteresisCount = 5;

				if (! force_color) {
					IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_NIGHT);
					IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM_NIGHT, SENSOR_FRAME_RATE_DEN);
				} else {
					HAPLogDebug(&logObject, "[%s] force_color is on",
						get_curr_timestr((char *) &tmstr));
				}
				sample_set_IRCUT(0);
			}
		} else if (avgExp < EXP_DAY_THRESHOLD) {
			if (pmode != IMPISP_RUNNING_MODE_DAY) {
				pmode = IMPISP_RUNNING_MODE_DAY;
				HAPLogDebug(&logObject, "[%s] avg exp is %d. switching to day mode",
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
				HAPLogDebug(&logObject, "[%s] avg exp is %d. turning on IR LEDs",
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
		} else if (ir_leds_active && avgExp < EXP_IR_OFF_THRESHOLD) {
			HAPLogDebug(&logObject, "[%s] avg exp is %d. turning off IR LEDs",
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

  prctl(PR_SET_NAME, "srtp_video");

  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  //for (i = 0; i < totalSaveCnt; i++) {
  // //TODO: in the future, check the mutex for changes to the stream here
  while(1){
    ret = IMP_Encoder_PollingStream(chnNum, 1000);
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout", chnNum);
      continue;
    }

    IMPEncoderStream stream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &stream, 1);
	if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed", chnNum);
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
    if(((int)(now - statime_sp[chnNum]) / 1000) >= 10){ // report bit rate every 10 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);

      HAPLogDebug(&logObject,"streamNum[%d]:FPS: %d,Bitrate: %d(kbps)",chnNum, (int)floor(fps),(int)floor(kbr) );
      
      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }
#endif
//TODO: for now just dump the data
#if 0
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed", chnNum);
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
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed", chnNum);
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

  prctl(PR_SET_NAME, "get_jpeg");


  ret = IMP_Encoder_StartRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StartRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  //for (i = 0; i < totalSaveCnt; i++) {
  // //TODO: in the future, check the mutex for changes to the stream here
  while(1){
    ret = IMP_Encoder_PollingStream(chnNum, 5000);
    if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_PollingStream(%d) timeout", chnNum);
      continue;
    }

    IMPEncoderStream stream;
    /* Get H264 or H265 Stream */
    ret = IMP_Encoder_GetStream(chnNum, &stream, 1);
	if (ret < 0) {
      HAPLogError(&logObject, "IMP_Encoder_GetStream(%d) failed", chnNum);
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
    if(((int)(now - statime_sp[chnNum]) / 1000) >= 10){ // report bit rate every 10 seconds
      double fps = (double)frmrate_sp[chnNum] / ((double)(now - statime_sp[chnNum]) / 1000);
      double kbr = (double)bitrate_sp[chnNum] * 8 / (double)(now - statime_sp[chnNum]);

      HAPLogDebug(&logObject,"streamNum[%d]:FPS: %d, Bitrate: %d(kbps)",chnNum, (int)floor(fps),(int)floor(kbr) );
      //fflush(stdout);

      frmrate_sp[chnNum] = 0;
      bitrate_sp[chnNum] = 0;
      statime_sp[chnNum] = now;
    }
#endif
	char * snap_path = "/tmp/snap.jpg";

//	HAPLogDebug(&logObject, "Locking snapshot mutex");
	if ( pthread_mutex_lock(&snapshot_mutex) != 0){
		HAPLogError(&logObject, "Locking snapshot mutex failed: %s", strerror(errno));
	}


	//TTTHAPLogDebug(&logObject, "Open Snap file %s ", snap_path);
	int snap_fd = open(snap_path, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (snap_fd < 0) {
		HAPLogError(&logObject, "failed: %s", strerror(errno));
	}
	else{
		//TTTHAPLogDebug(&logObject, "OK");

		int ret, nr_pack = stream.packCount;

		// //TTTHAPLogDebug(&logObject, "----------packCount=%d, stream.seq=%u start----------", stream.packCount, stream.seq);
		for (i = 0; i < nr_pack; i++) {
		// //TTTHAPLogDebug(&logObject, "[%d]:%10u,%10lld,%10u,%10u,%10u", i, stream.pack[i].length, stream.pack[i].timestamp, stream.pack[i].frameEnd, *((uint32_t *)(&stream.pack[i].nalType)), stream.pack[i].sliceType);
			IMPEncoderPack *pack = &stream.pack[i];
			if(pack->length){
				uint32_t remSize = stream.streamSize - pack->offset;
				if(remSize < pack->length){
					ret = write(snap_fd, (void *)(stream.virAddr + pack->offset), remSize);
					if (ret != remSize) {
						//TTTHAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].remSize(%d) error:%s", ret, i, remSize, strerror(errno));
						// return -1;
					}
					ret = write(snap_fd, (void *)stream.virAddr, pack->length - remSize);
					if (ret != (pack->length - remSize)) {
						//TTTHAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].(length-remSize)(%d) error:%s", ret, i, (pack->length - remSize), strerror(errno));
						// return -1;
					}
				}else {
					ret = write(snap_fd, (void *)(stream.virAddr + pack->offset), pack->length);
					if (ret != pack->length) {
						//TTTHAPLogDebug(&logObject, "stream write ret(%d) != pack[%d].length(%d) error:%s", ret, i, pack->length, strerror(errno));
						// return -1;
					}
				}
			}
		}
		close(snap_fd);

	////TTTHAPLogDebug(&logObject, "----------packCount=%d, stream.seq=%u end----------", stream.packCount, stream.seq);
	}

//	HAPLogDebug(&logObject, "Unlocking snapshot mutex");
	if ( pthread_mutex_unlock(&snapshot_mutex) != 0){
		HAPLogError(&logObject, "Unocking snapshot mutex failed: %s", strerror(errno));
	}

    IMP_Encoder_ReleaseStream(chnNum, &stream);
  }

  ret = IMP_Encoder_StopRecvPic(chnNum);
  if (ret < 0) {
    HAPLogError(&logObject, "IMP_Encoder_StopRecvPic(%d) failed", chnNum);
    return ((void *)-1);
  }

  return ((void *)0);
}


extern int IMP_OSD_SetPoolSize(int size);
//extern int IMP_Encoder_SetPoolSize(int size);

extern void *sample_ivs_move_get_result_process(void *arg);

int initVideoPipeline(){

	int ret = 0;
	IMPSensorInfo sensor_info;

	IMP_OSD_SetPoolSize(512*1024);
	IMP_Log_Set_Option(IMP_LOG_OP_ALL);

	HAPLogInfo(&logObject, "initVideoPipeline start");


	ret = IMP_ISP_Open();
	if(ret < 0){
		HAPLogError(&logObject,"failed to open ISP");
		return -1;
	}
	
	HAPLogDebug(&logObject, "IMP_System_Init Finish. ");

	memset(&sensor_info, 0, sizeof(IMPSensorInfo));
	memcpy(sensor_info.name, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.cbus_type = SENSOR_CUBS_TYPE;
	memcpy(sensor_info.i2c.type, SENSOR_NAME, sizeof(SENSOR_NAME));
	sensor_info.i2c.addr = SENSOR_I2C_ADDR;

	HAPLogDebug(&logObject, "IMP_ISP_AddSensor start");
	ret = IMP_ISP_AddSensor(&sensor_info);
	if(ret < 0){
		HAPLogError(&logObject,"failed to AddSensor");
		return -1;
	}

	HAPLogDebug(&logObject, "IMP_ISP_EnableSensor start");
	ret = IMP_ISP_EnableSensor();
	if(ret < 0){
		HAPLogError(&logObject,"failed to EnableSensor");
		return -1;
	}

	HAPLogDebug(&logObject, "IMP_System_Init start");
	ret = IMP_System_Init();
	if(ret < 0){
		HAPLogError(&logObject,"IMP_System_Init failed");
		return -1;
	}


	HAPLogDebug(&logObject, "IMP_ISP_EnableTuning start");
#if 1

	/* enable turning, to debug graphics */
	ret = IMP_ISP_EnableTuning();
	if(ret < 0){
		HAPLogError(&logObject,"IMP_ISP_EnableTuning failed");
		return -1;
	}
#endif
#if 0
    IMP_ISP_Tuning_SetContrast(128);
    IMP_ISP_Tuning_SetSharpness(128);
    IMP_ISP_Tuning_SetSaturation(128);
    IMP_ISP_Tuning_SetBrightness(128);
#endif


#if 1
	HAPLogDebug(&logObject, "IMP_ISP_Tuning_SetISPRunningMode start");
    ret = IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY);
    if (ret < 0){
        HAPLogError(&logObject,"failed to set running mode");
        return -1;
    }
#endif
#if 1
    ret = IMP_ISP_Tuning_SetSensorFPS(SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN);
    if (ret < 0){
        HAPLogError(&logObject,"failed to set sensor fps");
        return -1;
    }
#endif
	HAPLogDebug(&logObject, "ImpSystemInit success");


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

	HAPLogDebug(&logObject, "Setting up Framesource 0 ");
	ret = IMP_FrameSource_CreateChn(0 /*index*/, &fs0_chn_attr);
	if(ret < 0){
		HAPLogError(&logObject,"IMP_FrameSource_CreateChn(chn%d) error !", 0);
		return -1;
	}

	ret = IMP_FrameSource_SetChnAttr(0 /*index*/, &fs0_chn_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_SetChnAttr(chn%d) error !",  0);
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
			
	HAPLogDebug(&logObject, "Setting up Framesource 1 ");
	ret = IMP_FrameSource_CreateChn(1 /*index*/, &fs1_chn_attr);
	if(ret < 0){
		HAPLogError(&logObject,"IMP_FrameSource_CreateChn(chn%d) error !", 1);
		return -1;
	}

	ret = IMP_FrameSource_SetChnAttr(1 /*index*/, &fs1_chn_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_SetChnAttr(chn%d) error !",  1);
		return -1;
	}
#endif

	// flip the image 180deg if requested
	if (flip_image) {
		ret = IMP_ISP_Tuning_SetISPHflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		if (ret < 0){
			HAPLogError(&logObject,"failed to set horiz flip\n");
		}
		IMPISPTuningOpsMode pmode;
		ret = IMP_ISP_Tuning_GetISPHflip(&pmode);
		if (ret < 0){
			HAPLogError(&logObject,"failed to get horiz flip\n");
		}
		if (pmode != IMPISP_TUNING_OPS_MODE_ENABLE){
			HAPLogError(&logObject,"tried to horiz flip, but imp didn't take the change\n");

		}

		ret = IMP_ISP_Tuning_SetISPVflip(IMPISP_TUNING_OPS_MODE_ENABLE);
		if (ret < 0){
			HAPLogError(&logObject,"failed to set vert flip\n");
		}
		ret = IMP_ISP_Tuning_GetISPVflip(&pmode);
		if (ret < 0){
			HAPLogError(&logObject,"failed to get very flip\n");
		}
		if (pmode != IMPISP_TUNING_OPS_MODE_ENABLE){
			HAPLogError(&logObject,"tried to vert flip, but imp didn't take the change\n");

		}
	}

	HAPLogDebug(&logObject, "Setting up encoder group 0 ");

	ret = IMP_Encoder_CreateGroup( 0 /* index*/ );
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateGroup(%d) error !", 0);
		return -1;
	}


	IMPEncoderChnAttr enc0_channel0_attr;
	//TODO: experiment with different encoder rc modes
 	static const IMPEncoderRcMode S_RC_METHOD = IMP_ENC_RC_MODE_CBR; //IMP_ENC_RC_MODE_CAPPED_VBR; //IMP_ENC_RC_MODE_FIXQP; //IMP_ENC_RC_MODE_CAPPED_QUALITY;

	ret = IMP_Encoder_SetDefaultParam(&enc0_channel0_attr, IMP_ENC_PROFILE_AVC_MAIN, S_RC_METHOD,
			SENSOR_WIDTH, SENSOR_HEIGHT,
			SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN,
			SENSOR_FRAME_RATE_NUM * 2 / SENSOR_FRAME_RATE_DEN, // GOP length (2 secc)
			2, // uMaxSameSenceCnt ??
			(S_RC_METHOD == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1, // iInitialQP
			1000); // uTargetBitRate
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !", 0);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 0, &enc0_channel0_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !", 0);
		return -1;
	}

	ret = IMP_Encoder_RegisterChn(0 /* encGroup */, 0 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d", 0, 0, ret);
		return -1;
	}


	HAPLogDebug(&logObject, "Setting up encoder group 1: h264 ");

#if 1
	ret = IMP_Encoder_CreateGroup( 1 /* index*/ );
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateGroup(%d) error !", 1);
		return -1;
	}

	IMPEncoderChnAttr enc1_channel0_attr;

	ret = IMP_Encoder_SetDefaultParam(&enc1_channel0_attr, IMP_ENC_PROFILE_AVC_MAIN, S_RC_METHOD,
			SENSOR_WIDTH, SENSOR_HEIGHT,
			SENSOR_FRAME_RATE_NUM, SENSOR_FRAME_RATE_DEN,
			SENSOR_FRAME_RATE_NUM * 2 / SENSOR_FRAME_RATE_DEN, // GOP length (2 sec)
			2, // uMaxSameSenceCnt ??
			(S_RC_METHOD == IMP_ENC_RC_MODE_FIXQP) ? 35 : -1, // iInitialQP
			1000 ); // uTargetBitRate
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !", 1);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 1, &enc1_channel0_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !", 1);
		return -1;
	}

 	ret = IMP_Encoder_RegisterChn( 1 /* encGroup */, 1 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d", 1, 1, ret);
		return -1;
	}


	HAPLogDebug(&logObject, "Setting up encoder group 2: jpeg ");

	IMPEncoderChnAttr enc1_channel1_attr;

	#define JPEG_FRAME_RATE_NUM 1
	#define JPEG_FRAME_RATE_DEN 1

	ret = IMP_Encoder_SetDefaultParam(&enc1_channel1_attr, IMP_ENC_PROFILE_JPEG, IMP_ENC_RC_MODE_FIXQP,
			640, 360, // SENSOR_WIDTH, SENSOR_HEIGHT,
			JPEG_FRAME_RATE_NUM, JPEG_FRAME_RATE_DEN,
			0, 0, 25, 0);

	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_SetDefaultParam(%d) error !", 2);
		return -1;
	}

	ret = IMP_Encoder_CreateChn(/*encChn*/ 2, &enc1_channel1_attr);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_CreateChn(%d) error !", 2);
		return -1;
	}

	ret = IMP_Encoder_RegisterChn( 1 /* encGroup */, 2 /* encChn */);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_Encoder_RegisterChn(%d, %d) error: %d", 1, 2, ret);
		return -1;
	}

	HAPLogDebug(&logObject, "Setting up motion detection ");

	ret = IMP_IVS_CreateGroup(0);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_IVS_CreateGroup(%d) failed", 0);
		return -1;
	}
#endif

	HAPLogDebug(&logObject, "Binding video path for SRTP ");

	IMPCell	framesource0_chn =	{ DEV_ID_FS, 0, 0};
	IMPCell imp_encoder0 = { DEV_ID_ENC, 0, 0};

	IMPCell	framesource1_chn =	{ DEV_ID_FS, 1, 0};
	IMPCell ivs_cell = {DEV_ID_IVS, 0, 0};

	ret = IMP_System_Bind(&framesource1_chn, &imp_encoder0);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind FrameSource channel.0 output.0 and encoder0 failed");
		return -1;
	}
#if 1

	HAPLogDebug(&logObject, "Binding video path for motion ");
	ret = IMP_System_Bind(&framesource0_chn, &ivs_cell);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind FrameSource channel.1 output.0 and ivs0 failed");
		return -1;
	}
	IMPCell imp_encoder1 = { DEV_ID_ENC, 1, 0};
	HAPLogDebug(&logObject, "Binding video path for HKSV");
	ret = IMP_System_Bind(&ivs_cell, &imp_encoder1);
	if (ret < 0) {
		HAPLogError(&logObject,"Bind ivs0 channel%d and encoder1 failed",0);
		return -1;
	}

	// Do I need to bind the jpeg encoder??
	// I don't think so, sample-Encoder-h264-jpeg.c line 79 only binds the h264+jpeg encoding group, not the jpeg encoder
	// IMPCell imp_encoder2 = { DEV_ID_ENC, 2, 0};
	// HAPLogDebug(&logObject, "Binding video path for JPEG ");
	// ret = IMP_System_Bind(&ivs_cell, &imp_encoder2);
	// if (ret < 0) {
	// 	HAPLogError(&logObject,"Bind ivs0 channel%d and encoder2 failed",0);
	// 	return -1;
	// }

#endif
	HAPLogDebug(&logObject, "Starting Frame Sources ");

//	moving this to POSCameraController

//	ret = IMP_FrameSource_EnableChn(1);
//	if (ret < 0) {
//		HAPLogError(&logObject,"IMP_FrameSource_EnableChn(%d) error: %d", 0, ret);
//		return -1;
//	}

#if 1

	ret = IMP_FrameSource_EnableChn(0);
	if (ret < 0) {
		HAPLogError(&logObject,"IMP_FrameSource_EnableChn(%d) error: %d", 1, ret);
		return -1;
	}
#endif

	HAPLogDebug(&logObject, "Starting motion sensing ");

	IMP_IVS_MoveParam ivs_param;
	IMPIVSInterface *ivs_interface = NULL;
	int i = 0, j = 0;

	memset(&ivs_param, 0, sizeof(IMP_IVS_MoveParam));
	ivs_param.skipFrameCnt = 5;
	ivs_param.frameInfo.width = SENSOR_WIDTH;
	ivs_param.frameInfo.height = SENSOR_HEIGHT;
	ivs_param.roiRectCnt = 1;

	// TODO: experiment with sensitivity?
	// The sensitivity of motion detection is 0-4 for normal cameras
	for(i=0; i<ivs_param.roiRectCnt; i++){
	  ivs_param.sense[i] = 2;
	}

	/* HAPLogDebug(&logObject,"ivs_param.sense=%d, ivs_param.skipFrameCnt=%d, ivs_param.frameInfo.width=%d, ivs_param.frameInfo.height=%d", ivs_param.sense, ivs_param.skipFrameCnt, ivs_param.frameInfo.width, ivs_param.frameInfo.height); */
	//TODO: setup regions of interest
	for (j = 0; j < 1/*2*/; j++) {
		for (i = 0; i < 1/*2*/; i++) {
		  if((i==0)&&(j==0)){
			ivs_param.roiRect[j * 2 + i].p0.x = i * ivs_param.frameInfo.width /* / 2 */;
			ivs_param.roiRect[j * 2 + i].p0.y = j * ivs_param.frameInfo.height /* / 2 */;
			ivs_param.roiRect[j * 2 + i].p1.x = (i + 1) * ivs_param.frameInfo.width /* / 2 */ - 1;
			ivs_param.roiRect[j * 2 + i].p1.y = (j + 1) * ivs_param.frameInfo.height /* / 2 */ - 1;
			HAPLogDebug(&logObject,"(%d,%d) = ((%d,%d)-(%d,%d))", i, j, ivs_param.roiRect[j * 2 + i].p0.x, ivs_param.roiRect[j * 2 + i].p0.y,ivs_param.roiRect[j * 2 + i].p1.x, ivs_param.roiRect[j * 2 + i].p1.y);
		  }
		  else
		    {
		      	ivs_param.roiRect[j * 2 + i].p0.x = 0;//ivs_param.roiRect[0].p0.x;
			ivs_param.roiRect[j * 2 + i].p0.y = 0;//ivs_param.roiRect[0].p0.y;
			ivs_param.roiRect[j * 2 + i].p1.x = 0;//ivs_param.roiRect[0].p1.x;;
			ivs_param.roiRect[j * 2 + i].p1.y = 0;//ivs_param.roiRect[0].p1.y;;
			HAPLogDebug(&logObject,"(%d,%d) = ((%d,%d)-(%d,%d))", i, j, ivs_param.roiRect[j * 2 + i].p0.x, ivs_param.roiRect[j * 2 + i].p0.y,ivs_param.roiRect[j * 2 + i].p1.x, ivs_param.roiRect[j * 2 + i].p1.y);
		    }
		}
	}
	ivs_interface = IMP_IVS_CreateMoveInterface(&ivs_param);
	if (ivs_interface == NULL) {
		HAPLogError(&logObject, "IMP_IVS_CreateGroup(%d) failed", /*grp_num*/ 0);
		return -1;
	}

	ret = IMP_IVS_CreateChn(/*chn_num*/ 2, ivs_interface);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_CreateChn(%d) failed", /*chn_num*/ 2);
		return -1;
	}

	ret = IMP_IVS_RegisterChn(/*ivs grp_num*/ 0, /*chn_num*/ 2);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_RegisterChn(%d, %d) failed", /*grp_num*/ 0, /*chn_num*/ 2);
		return -1;
	}

	ret = IMP_IVS_StartRecvPic(/*chn_num*/ 2);
	if (ret < 0) {
		HAPLogError(&logObject, "IMP_IVS_StartRecvPic(%d) failed", /*chn_num*/ 2);
		return -1;
	}
	
	pthread_t ivs_tid;

	HAPLogInfo(&logObject, "Starting motion sensing thread ");
	if (pthread_create(&ivs_tid, NULL, sample_ivs_move_get_result_process, (void *) /*chn_num*/ 2) < 0) {
		HAPLogError(&logObject, "create sample_ivs_move_get_result_process failed");
		return -1;
	}

	int arg;

//	moving this to POSCameraController
//
//	pthread_t srtp_tid;
//
//	HAPLogInfo(&logObject, "Starting srtp capture thread ");
//	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 0));
//	ret = pthread_create(&srtp_tid, NULL, get_srtp_video_stream, (void *)arg);
//	if (ret < 0) {
//		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 0 ));
//	}

	pthread_t hksv_tid;

	HAPLogInfo(&logObject, "Starting hksv capture thread ");
	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 1));
	// TODO: in the future, change this to a hskv specific processing thread
	ret = pthread_create(&hksv_tid, NULL, get_srtp_video_stream, (void *)arg);
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 1 ));
	}

	pthread_t jpeg_tid;

	HAPLogInfo(&logObject, "Starting jpeg capture thread ");
	arg = (((IMP_ENC_PROFILE_AVC_MAIN >> 24) << 16) | (/*chn_num*/ 2));
	// TODO: in the future, change this to a jpeg specific processing thread
	ret = pthread_create(&jpeg_tid, NULL, get_jpeg_stream, (void *)arg);
	if (ret < 0) {
		HAPLogError(&logObject, "Create ChnNum%d get_srtp_video_stream failed", (/*chn_num*/ 2 ));
	}


	// start thread for activating night mode & IR cut filter
	HAPLogInfo(&logObject, "Starting thread for activating night mode & IR cut filter ");
	pthread_t thread_info;
	pthread_create(&thread_info, NULL, sample_soft_photosensitive_ctrl, NULL);
	if (ret < 0) {
		HAPLogError(&logObject, "Failed to start thread for activating night mode & IR cut filter");
	}

	//TODO: join these threads at somepoint




}
