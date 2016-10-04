//ģ��Android 4.2.2Դ���е�EventHub.cpp

#include <jni.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <android/keycodes.h>
#include "types.h"
#include "Debug.h"
#include "Input.h"
#include "linux_input.h"

#define INPUT_DEVICE_FILE_PATH "/dev/input"

/* this macro is used to tell if "bit" is set in "array"
 * it selects a byte from the array, and does a boolean AND
 * operation with a byte that only has the relevant bit set.
 * eg. to check for the 12th bit, we do (array[1] & 1<<4)
 */
#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

#define sizeof_bit_array(bits)  ((bits + 7) / 8)

InputDevice* InputDevice::s_pInstance = NULL;

InputDevice& InputDevice::GetInstance()
{
	if(s_pInstance == NULL)
	{
		static Cleaner cl;
		s_pInstance = new InputDevice();
	}
	return *s_pInstance;
}

void InputDevice::DeleteInstance()
{
	if(s_pInstance)
	{
		delete s_pInstance;
		s_pInstance = NULL;
	}
}

//����android�����е�EventHub::scanDir()
InputDevice::InputDevice()
{
	m_iKeyboardFDNumber = m_iTouchScreenFDNumber = 0;
	memset(&m_InputDevFD, 0, sizeof(m_InputDevFD));

	m_pTouchInfo = NULL;
	m_ProtoType = PROTO_TYPE_UNKNOW; //PROTO_TYPE_UNKNOW��ʾЭ������δ֪
	ClearTouchInfoArray(m_MultiTouchPointInfo);
	BuildScanCodeMapping();
	
	if(pipe(m_ThreadEventPipe) < 0)
		return;

	m_InputDevFD.EventPipe.fd = m_ThreadEventPipe[0];
	m_InputDevFD.EventPipe.events = POLLIN;

	//����/dev/inputĿ¼
	DIR* dir = opendir(INPUT_DEVICE_FILE_PATH);
	if(dir == NULL)
		return;

	struct dirent *de;
	while(de = readdir(dir))
	{	//�ų�.��..�ļ�
		if(de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;
		
		char lpszPathName[PATH_MAX] = INPUT_DEVICE_FILE_PATH"/";
		strcat(lpszPathName, de->d_name);

		//���Դ��ļ�
		int fd = open(lpszPathName, O_RDWR);
		if(fd < 0)
			continue;

		//����Ǽ��̻������豸�����fd��¼����������ر�fd
		auto iType = AddDevice(fd);
		if(iType)
		{
			LOGI("found device type = %d path = %s", iType, lpszPathName);
		}
		else
		{
			close(fd);
		}
	}
	closedir(dir);

	int iAllInputFDNumber = 0;
	m_pAllInputFD = new pollfd[1 + m_iKeyboardFDNumber + m_iTouchScreenFDNumber];	//1���ܵ���һ����̣�һ�鴥����
	m_pAllInputFD[iAllInputFDNumber++] = m_InputDevFD.EventPipe;
	for(auto i = 0; i < m_iKeyboardFDNumber; i++)
	{
		m_pAllInputFD[iAllInputFDNumber++] = m_InputDevFD.m_KeyboardPollFD[i];
	}

	for(auto i = 0; i < m_iTouchScreenFDNumber; i++)
	{
		m_pAllInputFD[iAllInputFDNumber++] = m_InputDevFD.m_TouchScreenPollFD[i];
	}
}

InputDevice::~InputDevice()
{
	LOGI("InputDevice::~InputDevice()");

	Cancel();
	close(m_ThreadEventPipe[1]);

	for(auto i = 0; i < m_iKeyboardFDNumber; i++) 
	{
		close(m_InputDevFD.m_KeyboardPollFD[i].fd);
	}

	for(auto i = 0; i < m_iTouchScreenFDNumber; i++) 
	{
		close(m_InputDevFD.m_TouchScreenPollFD[i].fd);
	}

	m_iKeyboardFDNumber = m_iTouchScreenFDNumber = 0;
	memset(&m_InputDevFD, 0, sizeof(m_InputDevFD));

	if(m_pTouchInfo)
	{
		delete [] m_pTouchInfo;
		m_pTouchInfo = NULL;
	}

	if(m_pAllInputFD)
	{
		delete [] m_pAllInputFD;
		m_pAllInputFD = NULL;
	}
	ClearTouchInfoArray(m_MultiTouchPointInfo);
}

bool InputDevice::containsNonZeroByte(const uint8_t* array, uint32_t startIndex, uint32_t endIndex) 
{	//��androidԴ�����հ�
	const uint8_t* end = array + endIndex;
	array += startIndex;
	while (array != end) 
	{
		if (*(array++) != 0) 
		{
			return true;
		}
	}
	return false;
}

void InputDevice::BuildScanCodeMapping()
{
	UINT KeyCodeMapping[][2] = 
	{
		AKEYCODE_HOME,			KEY_HOME,
		AKEYCODE_BACK,			KEY_BACK,
		AKEYCODE_CALL,			0,
		AKEYCODE_ENDCALL,		0,
		AKEYCODE_0,			 	KEY_0,
		AKEYCODE_1,			 	KEY_1,
		AKEYCODE_2,			 	KEY_2,
		AKEYCODE_3,			 	KEY_3,
		AKEYCODE_4,			 	KEY_4,
		AKEYCODE_5,			 	KEY_5,
		AKEYCODE_6,			 	KEY_6,
		AKEYCODE_7,			 	KEY_7,
		AKEYCODE_8,			 	KEY_8,
		AKEYCODE_9,			 	KEY_9,
		AKEYCODE_VOLUME_UP,		KEY_VOLUMEUP,
		AKEYCODE_VOLUME_DOWN,	KEY_VOLUMEDOWN,
		AKEYCODE_POWER,			KEY_POWER,
		AKEYCODE_CAMERA,		KEY_CAMERA,
		AKEYCODE_A,				KEY_A,
		AKEYCODE_B,				KEY_B,
		AKEYCODE_C,				KEY_C,
		AKEYCODE_D,				KEY_D,
		AKEYCODE_E,				KEY_E,
		AKEYCODE_F,				KEY_F,
		AKEYCODE_G,				KEY_G,
		AKEYCODE_H,				KEY_H,
		AKEYCODE_I,				KEY_I,
		AKEYCODE_J,				KEY_J,
		AKEYCODE_K,				KEY_K,
		AKEYCODE_L,				KEY_L,
		AKEYCODE_M,				KEY_M,
		AKEYCODE_N,				KEY_N,
		AKEYCODE_O,				KEY_O,
		AKEYCODE_P,				KEY_P,
		AKEYCODE_Q,				KEY_Q,
		AKEYCODE_R,				KEY_R,
		AKEYCODE_S,				KEY_S,
		AKEYCODE_T,				KEY_T,
		AKEYCODE_U,				KEY_U,
		AKEYCODE_V,				KEY_V,
		AKEYCODE_W,				KEY_W,
		AKEYCODE_X,				KEY_X,
		AKEYCODE_Y,				KEY_Y,
		AKEYCODE_Z,				KEY_Z,
		AKEYCODE_COMMA,			KEY_COMMA,
		AKEYCODE_PERIOD,		0,
		AKEYCODE_TAB,			KEY_TAB,
		AKEYCODE_SPACE,			KEY_SPACE,
		AKEYCODE_ENTER,			KEY_ENTER,
		AKEYCODE_DEL,			KEY_DELETE,
		AKEYCODE_MINUS,			KEY_MINUS,
		AKEYCODE_EQUALS,		KEY_EQUAL,
		AKEYCODE_LEFT_BRACKET,	KEY_LEFTBRACE,
		AKEYCODE_RIGHT_BRACKET, KEY_RIGHTBRACE,
		AKEYCODE_BACKSLASH,		KEY_BACKSLASH,
		AKEYCODE_SEMICOLON,		KEY_SEMICOLON,
		AKEYCODE_SLASH,			KEY_SLASH,
		AKEYCODE_AT,			0,
		AKEYCODE_PLUS,			0,
		AKEYCODE_MENU,			KEY_MENU,
		AKEYCODE_PAGE_UP,		KEY_PAGEUP,
		AKEYCODE_PAGE_DOWN,		KEY_PAGEDOWN,
	};
	const int KeyCodeMappingSize = sizeof(KeyCodeMapping) / sizeof(KeyCodeMapping[0]);

	memset(m_KeyboardScanCodeMapping, 0, sizeof(m_KeyboardScanCodeMapping));

	for(int i = 0; i < KeyCodeMappingSize; i++)
	{
		if(KeyCodeMapping[i][1] < MAX_KEY_NUMBER)
			m_KeyboardScanCodeMapping[KeyCodeMapping[i][1]] = (BYTE)KeyCodeMapping[i][0];
	}
}

int InputDevice::AddDevice(int fd)
{	
	uint8_t keyBitmask[(KEY_MAX + 1) / 8] = {0};
	uint8_t absBitmask[(ABS_MAX + 1) / 8] = {0};
		
	ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBitmask)), keyBitmask);
	ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBitmask)), absBitmask);
  
	int iDeviceType = 0;
  
  //�ж��Ƿ��Ǽ����豸
	// See if this is a keyboard.  Ignore everything in the button range except for
	// joystick and gamepad buttons which are handled like keyboards for the most part.
	bool haveKeyboardKeys = containsNonZeroByte(keyBitmask, 0, sizeof_bit_array(BTN_MISC))
            || containsNonZeroByte(keyBitmask, sizeof_bit_array(KEY_OK),
                    sizeof_bit_array(KEY_MAX + 1));
	bool haveGamepadButtons = containsNonZeroByte(keyBitmask, sizeof_bit_array(BTN_MISC),
                    sizeof_bit_array(BTN_MOUSE))
            || containsNonZeroByte(keyBitmask, sizeof_bit_array(BTN_JOYSTICK),
                    sizeof_bit_array(BTN_DIGI));
	if (haveKeyboardKeys || haveGamepadButtons) 
	{
		iDeviceType = 1;		//�����豸
	}

  //�ж��Ƿ��Ǵ������豸
	// See if this is a touch pad.
	// Is this a new modern multi-touch driver?
	if (test_bit(ABS_MT_POSITION_X, absBitmask)
            && test_bit(ABS_MT_POSITION_Y, absBitmask)) 
	{
		// Some joysticks such as the PS3 controller report axes that conflict
		// with the ABS_MT range.  Try to confirm that the device really is
		// a touch screen.
		if (test_bit(BTN_TOUCH, keyBitmask) || !haveGamepadButtons) 
		{
			iDeviceType = 2;		//��㴥�����豸
		}
	}
	else if(test_bit(BTN_TOUCH, keyBitmask) && test_bit(ABS_X, absBitmask) && test_bit(ABS_Y, absBitmask)) 
	{	// Is this an old style single-touch driver?
		//�ݲ����ǵ��㴥���豸
		//iDeviceType = 2;		//���㴥�����豸
		LOGI("------>>>>>This is an old style single-touch driver");
	}

	if(iDeviceType == 1 && m_iKeyboardFDNumber < MAX_FD_NUMBER)
	{
		m_InputDevFD.m_KeyboardPollFD[m_iKeyboardFDNumber].fd = fd;
		m_InputDevFD.m_KeyboardPollFD[m_iKeyboardFDNumber].events = POLLIN;
		m_iKeyboardFDNumber++;
	}

	if(iDeviceType == 2 && m_iTouchScreenFDNumber < MAX_FD_NUMBER)
	{
		m_InputDevFD.m_TouchScreenPollFD[m_iTouchScreenFDNumber].fd = fd;
		m_InputDevFD.m_TouchScreenPollFD[m_iTouchScreenFDNumber].events = POLLIN;
		m_iTouchScreenFDNumber++;
	}
	
	return iDeviceType;
}

int InputDevice::ReadInput(bool bGetKeyDownOnly, bool bIsIncludeTouchScreenDevice, int ReturnValues[])
{
	LOGI("ReadInput(include touch screen = %d) begin", bIsIncludeTouchScreenDevice);
	bool bRet = false;
	int iPointId = -1;
	struct pollfd *AllInputFD = m_pAllInputFD;
	int iAllInputFDNumber = 0;
	
	if(bIsIncludeTouchScreenDevice == false)
		iAllInputFDNumber = 1 + m_iKeyboardFDNumber;
	else
		iAllInputFDNumber = 1 + m_iKeyboardFDNumber + m_iTouchScreenFDNumber;

	while(1)
	{	//��getevent��value��Ϊ1��ʱ�򣬼����ȴ����
		//�ȴ����еļ����豸��һ�����ڼ���˳��Ĺܵ�
		auto pollres = poll(AllInputFD, iAllInputFDNumber, -1);

		if(AllInputFD[0].revents & POLLIN)
		{	//����ǹܵ����±�Ϊ0���ɶ��ˣ�˵���Ѿ��������˳���Ϣ
			close(m_ThreadEventPipe[0]);
			LOGI("Read Input Thread Need Quit");
			return -1;
		}
		
		for(auto i = 0; i < m_iKeyboardFDNumber; i++)
		{
			if(AllInputFD[1 + i].revents & POLLIN) 
			{
				struct input_event event;
				auto res = read(AllInputFD[1 + i].fd, &event, sizeof(event));
				if(res < (int)sizeof(event))
				{	//������
					continue;
				}
				if(event.type == EV_KEY)
				{	//������Ϣ
					LOGI("from %d get key event %d %d(%d) %d", i, event.type, event.code, m_KeyboardScanCodeMapping[event.code], event.value);

					if(bGetKeyDownOnly && event.value != 1)
						continue;

					//��һ���ֽ����С��100��˵���Ǽ�����Ϣ
					ReturnValues[0] = i;
					ReturnValues[1] = m_KeyboardScanCodeMapping[event.code];
					ReturnValues[2] = event.value;
					ReturnValues[3] = 0;
					ReturnValues[4] = 0;

					return event.code;
				}
			}
		}

		for(auto i = 0; i < m_iTouchScreenFDNumber; i++)
		{
			if(m_pTouchInfo == NULL)
				m_pTouchInfo = new STouchInfo[m_iTouchScreenFDNumber];

			int iReturnValue = -1;

			if(AllInputFD[1 + m_iKeyboardFDNumber + i].revents & POLLIN) 
			{
				struct input_event event;
				auto res = read(AllInputFD[1 + m_iKeyboardFDNumber + i].fd, &event, sizeof(event));
				if(res < (int)sizeof(event))
				{	//������
					continue;
				}

				UINT &rCurrentSlot = m_pTouchInfo[i].m_nCurrentSlot;
				rCurrentSlot = rCurrentSlot % MAX_SLOT_NUMBER;
				STouchInfo::STouchInfoSlot *pSlot = &(m_pTouchInfo[i].m_Slot[rCurrentSlot]);

				if(event.type == EV_ABS)
				{	
					switch(event.code)
					{
					case ABS_MT_SLOT:
						if(pSlot->bIsModified)
						{
							LOGI("ABS_MT_SLOT Dev %d, Slot %d, x=%d, y=%d, id=%d", i, rCurrentSlot, pSlot->iX, pSlot->iY, pSlot->iTrackingId);
							iReturnValue = ReturnValues[0] = i + 100;		//��һ���ֽ��������100��˵���Ǵ�������Ϣ
							ReturnValues[1] = rCurrentSlot;
							ReturnValues[2] = pSlot->iX;
							ReturnValues[3] = pSlot->iY;
							ReturnValues[4] = pSlot->iTrackingId;

							if(ReturnValues[4] < 0)
								pSlot->Clear();
							pSlot->bIsModified = false;
						}

						rCurrentSlot = event.value;
						break;
					case ABS_MT_POSITION_X:
					//case ABS_X:
						pSlot->iX = event.value;
						pSlot->bIsModified = true;
						break;
					case ABS_MT_POSITION_Y:
					//case ABS_Y:
						pSlot->iY = event.value;
						pSlot->bIsModified = true;
						break;
					case ABS_MT_TRACKING_ID:
						pSlot->iTrackingId = event.value;
						pSlot->bIsModified = true;
						break;
					}
				}
				else if(event.type == EV_SYN)
				{
					switch(event.code)
					{
					case SYN_MT_REPORT:
						m_ProtoType = PROTO_TYPE_A;
						AddPointToTouchInfoArray(m_pTouchInfo[i].m_CurTouchInfo, pSlot);
						bRet = IsTouchPointMove(m_pTouchInfo[i].m_LatestTouchInfo, pSlot);//ֻ�иõ㷢���ƶ��˲����Ϸ���
						if(pSlot->bIsModified && bRet)
						{
							LOGI("SYN_MT_REPORT (Move) Dev %d, Slot %d, x=%d, y=%d, id=%d", i, rCurrentSlot, pSlot->iX, pSlot->iY, pSlot->iTrackingId);
							iReturnValue = ReturnValues[0] = i + 100;
							ReturnValues[1] = pSlot->iTrackingId;//��ָID
							ReturnValues[2] = pSlot->iX;
							ReturnValues[3] = pSlot->iY;
							ReturnValues[4] = pSlot->iTrackingId;//-1��ʾ���ֻ�̧��

							if(ReturnValues[4] < 0)
								pSlot->Clear();
							pSlot->bIsModified = false;
						}
						break;
					case SYN_REPORT:
						if (PROTO_TYPE_A == m_ProtoType)
						{
							LOGI("PROTO_TYPE_A SYN_REPORT Dev %d, Slot %d, x=%d, y=%d, id=%d", i, rCurrentSlot, pSlot->iX, pSlot->iY, pSlot->iTrackingId);
							//������type Aģʽ��,�յ�SYN_REPORT��Ϣ����ȷ���Ƿ�����ָ̧��
							iPointId = GetTouchUpPoint(m_pTouchInfo[i].m_CurTouchInfo, m_pTouchInfo[i].m_LatestTouchInfo);
							if (-1 != iPointId)
							{
								iReturnValue = ReturnValues[0] = i + 100;
								ReturnValues[1] = iPointId;//��ָID
								ReturnValues[2] = 0;
								ReturnValues[3] = 0;
								ReturnValues[4] = -1;//-1��ʾ���ֻ�̧��
							}
							//����ǰ�Ĵ�����Ϣ���鸴�Ƶ����һ�εĴ�����Ϣ�����У�����յ�ǰ�Ĵ�����Ϣ����
							CopyTouchInfoArray(m_pTouchInfo[i].m_CurTouchInfo, m_pTouchInfo[i].m_LatestTouchInfo);
						}
						else
						{
							m_ProtoType = PROTO_TYPE_B;
							if(pSlot->bIsModified)
							{
								LOGI("PROTO_TYPE_B SYN_REPORT Dev %d, Slot %d, x=%d, y=%d, id=%d", i, rCurrentSlot, pSlot->iX, pSlot->iY, pSlot->iTrackingId);
								iReturnValue = ReturnValues[0] = i + 100;
								ReturnValues[1] = rCurrentSlot;
								ReturnValues[2] = pSlot->iX;
								ReturnValues[3] = pSlot->iY;
								ReturnValues[4] = pSlot->iTrackingId;
								
								//�˴���������pSlot�����ݣ���Ϊ�������ָ�����ˣ�Ҳ�п��ܻ������������������Ӷ������touchdownevent����������Ϊ-1�������
// 								[    7541.046314] EV_ABS       ABS_MT_TOUCH_MAJOR   00000001
// 								[    7541.046314] EV_ABS       ABS_MT_PRESSURE      0000000c
// 								[    7541.046344] EV_SYN       SYN_REPORT           00000000
// 								[    7541.066457] EV_ABS       ABS_MT_TRACKING_ID   ffffffff   //��ָ̧��
// 								[    7541.066457] EV_SYN       SYN_REPORT           00000000
// 
// 								[    7541.155394] EV_ABS       ABS_MT_TRACKING_ID   000006cd
// 								[    7541.155394] EV_ABS       ABS_MT_TOUCH_MAJOR   00000003
// 								[    7541.155424] EV_ABS       ABS_MT_POSITION_Y    00000423	//ֻ��Y��������ˣ�X����û�и��µ��¼�
// 								[    7541.155424] EV_ABS       ABS_MT_PRESSURE      0000001d
// 								[    7541.155455] EV_SYN       SYN_REPORT           00000000

// 								if(ReturnValues[4] < 0)
// 									pSlot->Clear();
								pSlot->bIsModified = false;
							}
						}
						break;
					}
				}
				else if(event.type == EV_KEY)
				{
					switch(event.code)
					{
					case BTN_TOUCH:
						if(event.value == 1)	//down
						{
						}
						else if(event.value == 0)	//up
						{
							pSlot->iTrackingId = -1;
							pSlot->bIsModified = true;
						}
						break;
					case KEY_HOME:
					case KEY_BACK:
					case KEY_MENU:
						//����С��2S��ʵ�尴��
						ReturnValues[0] = i;
						ReturnValues[1] = m_KeyboardScanCodeMapping[event.code];
						ReturnValues[2] = event.value;
						ReturnValues[3] = 0;
						ReturnValues[4] = 0;
						pSlot->bIsModified = false;
						return event.code;
						break;
					default:
						break;
					}
				}
			}		//if touch screen message comes

			if(iReturnValue >= 0)
				return iReturnValue;
		}	//for all touch screen devices
	}	//main loop

	return 0;
}

int InputDevice::ReadKeyboard(bool bGetKeyDownOnly)
{
	int Dummy[5];
	bool bIsIncludeTouchScreenDevice = false;
	if (PROTO_TYPE_UNKNOW == m_ProtoType){
		bIsIncludeTouchScreenDevice = true;
	}else{
		bIsIncludeTouchScreenDevice = false;
		LOGI("-----m_ProtoType = %d", m_ProtoType);
	}
	return ReadInput(bGetKeyDownOnly, bIsIncludeTouchScreenDevice, Dummy);
}

int InputDevice::WritePointerInput(int iAction, int iX, int iY, int iFingerId, int iTime)
{
	const UINT nMaxFingerId = 10;

	if(m_iTouchScreenFDNumber <=0)
		return -1;

	if((UINT)iFingerId >= nMaxFingerId)
		return -1;

	auto iPointerFD = m_pAllInputFD[1 + m_iKeyboardFDNumber].fd;
	if (PROTO_TYPE_A == m_ProtoType)
	{//Type Aģʽ
		WritePointerInputTypeA(iPointerFD, iAction, iX, iY, iFingerId);
	} 
	else
	{//Ĭ��ΪType Bģʽ
		static int iTrackingIdAllocated = 32768, iTrackingId[nMaxFingerId] = {0};

		LOGI("InputDevice::WritePointerInput %d %d finger=%d", iX, iY, iFingerId);
		switch(iAction)
		{
		case 0:		//touch_down
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_SLOT, iFingerId);
			iTrackingId[iFingerId] = iTrackingIdAllocated;
			iTrackingIdAllocated = ((iTrackingIdAllocated + 1) % 65536);

			if(iFingerId == 0)
			{
				WriteEvent(iPointerFD, EV_KEY, BTN_TOUCH, 1);
				WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
			}

			WriteEvent(iPointerFD, EV_ABS, ABS_MT_TRACKING_ID, iTrackingId[iFingerId]);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_X, iX);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_Y, iY);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_PRESSURE, 0x50);//��Ӵ�����ѹ��ϵ�����������ֻ�������ʱ�򣬿��ܻ����©������
			WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
			break;
		case 1:		//touch_move
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_SLOT, iFingerId);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_TRACKING_ID, iTrackingId[iFingerId]);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_X, iX);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_Y, iY);
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_PRESSURE, 0x50);
			WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
			break;
		case 2:		//touch_up
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_SLOT, iFingerId);
			iTrackingId[iFingerId] = 0;
			WriteEvent(iPointerFD, EV_ABS, ABS_MT_TRACKING_ID, -1);
			WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);

			if(iFingerId == 0)
			{
				WriteEvent(iPointerFD, EV_KEY, BTN_TOUCH, 0);
				WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
			}
			break;
		default:
			break;
		}
	}

	

	//delay
	if(iTime > 0)
	{
		if(iTime >= 1000)
			sleep(iTime / 1000);

		struct timeval delay_usec = { 0 };
		delay_usec.tv_usec = (iTime % 1000) * 1000;
		select(0, NULL, NULL, NULL, &delay_usec);
	}

	return 0;
}

void InputDevice::WritePointerInputTypeA(int iPointerFD, int iAction, int iX, int iY, int iFingerId)//Type AЭ��ģʽ��¼�ƻط�
{
	LOGI("InputDevice::WritePointerInputTypeA %d %d finger=%d", iX, iY, iFingerId);
	STouchInfo::STouchInfoSlot pSlot;
	pSlot.Clear();
	pSlot.iTrackingId = iFingerId;
	pSlot.iX = iX;
	pSlot.iY = iY;

	switch(iAction)
	{
	case 0:		//touch_down
	case 1:		//touch_move
		AddPointToTouchInfoArray(m_MultiTouchPointInfo, &pSlot);
		break;
	case 2:		//touch_up
		DeleteTouchPointFromArray(m_MultiTouchPointInfo, &pSlot);
		break;
	default:
		break;
	}

	//�����¼�
	int iNum = GetTouchPointNum(m_MultiTouchPointInfo);
	if (0 == iNum)
	{
		//���������һ����ָ̧����
		WriteEvent(iPointerFD, EV_KEY, BTN_TOUCH, 0);
		WriteEvent(iPointerFD, EV_SYN, SYN_MT_REPORT, 0);
		WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
	}
	else
	{
		//TODO:������ָ��һ�ΰ���
		if (1 == iNum)//����ָ
		{
			WriteEvent(iPointerFD, EV_KEY, BTN_TOUCH, 1);
		}
		
		for (int i = 0; i < MAX_SLOT_NUMBER; i++)
		{
			if (-1 != m_MultiTouchPointInfo[i].iTrackingId)
			{
				WriteEvent(iPointerFD, EV_ABS, ABS_MT_TRACKING_ID, m_MultiTouchPointInfo[i].iTrackingId);
				WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_X, m_MultiTouchPointInfo[i].iX);
				WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_Y, m_MultiTouchPointInfo[i].iY);
				WriteEvent(iPointerFD, EV_SYN, SYN_MT_REPORT, 0);
			}
		}

		WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);
	}

}

int InputDevice::WriteEvent(int fd, int iType, int iCode, int iValue)
{
	struct input_event event;
	gettimeofday(&(event.time),NULL);
	event.type = iType;
	event.code = iCode;
	event.value = iValue;
	LOGI("WriteEvent %d %d %d", iType, iCode, iValue);
	return write(fd, &event, sizeof(event));
}

int InputDevice::Cancel()
{
	write(m_ThreadEventPipe[1], &m_iKeyboardFDNumber, sizeof(m_iKeyboardFDNumber));	//���дһ��ֵ��pipe���õȴ����߳��˳�
}


/************************************************************************/
/*  Type AЭ��ģʽ�µķ���                                                             
/************************************************************************/
void InputDevice::AddPointToTouchInfoArray(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot)//���������Ϣ��ӵ���㴥�ص�������
{
	if (-1 == pSlot->iTrackingId)//-1��ʾ�õ������һ����ָ̧�𣬹�ֱ�ӷ���
	{
		return;
	}

	//���Ȳ鿴��id�Ƿ��Ѿ����������м�¼��
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if (pSlot->iTrackingId == TouchInfoArray[i].iTrackingId)
		{
			TouchInfoArray[i].iTrackingId = pSlot->iTrackingId;
			TouchInfoArray[i].iX = pSlot->iX;
			TouchInfoArray[i].iY = pSlot->iY;
			return;
		}
	}

	//û���ҵ���������ڵ�һ�����е�λ����
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if (-1 == TouchInfoArray[i].iTrackingId)
		{
			TouchInfoArray[i].iTrackingId = pSlot->iTrackingId;
			TouchInfoArray[i].iX = pSlot->iX;
			TouchInfoArray[i].iY = pSlot->iY;
			return;
		}
	}

}

void InputDevice::CopyTouchInfoArray(STouchPointInfo TouchInfoArrayFrom[], STouchPointInfo TouchInfoArrayTo[])//���ƶ�㴥������
{
	ClearTouchInfoArray(TouchInfoArrayTo);
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if (-1 != TouchInfoArrayFrom[i].iTrackingId)
		{
			TouchInfoArrayTo[i].iTrackingId = TouchInfoArrayFrom[i].iTrackingId;
			TouchInfoArrayTo[i].iX = TouchInfoArrayFrom[i].iX;
			TouchInfoArrayTo[i].iY = TouchInfoArrayFrom[i].iY;
		}
	}
	ClearTouchInfoArray(TouchInfoArrayFrom);
}

bool InputDevice::IsTouchPointMove(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot)//�жϸô����Ƿ��з����ƶ�
{
	if (-1 == pSlot->iTrackingId)//-1��ʾ�õ������һ����ָ̧�𣬹�ֱ�ӷ���false
	{
		return false;
	}

	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if ((pSlot->iTrackingId == TouchInfoArray[i].iTrackingId)
			&&(pSlot->iX == TouchInfoArray[i].iX)
			&&(pSlot->iY == TouchInfoArray[i].iY))
		{
			return false;
		}
	}
	return true;
}

void InputDevice::ClearTouchInfoArray(STouchPointInfo TouchInfoArray[])//��ն�㴥�ص�����
{
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		TouchInfoArray[i].Clear();
	}
}

int InputDevice::GetTouchUpPoint(STouchPointInfo TouchInfoArrayCur[], STouchPointInfo TouchInfoArrayLatest[])//��ȡ��ָ̧��ĵ㣬��û����ָ̧���򷵻�-1
{
	//��TouchInfoArrayLatest�е�����TouchInfoArrayCur�еĴ��㣬����Ϊ��̧�����ָ
	int iId = -1;
	int iRetVal = -1;
	bool bIsExist = false;
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		bIsExist = false;
		iId = TouchInfoArrayLatest[i].iTrackingId;
		//���Ҹ�id�Ƿ���TouchInfoArrayCur��
		for (int j = 0; j < MAX_SLOT_NUMBER; j++)
		{
			if (iId == TouchInfoArrayCur[j].iTrackingId)
			{
				bIsExist = true;
				break;
			}
		}

		if (!bIsExist)
		{
			iRetVal = iId;
			break;
		}
	}

	return iRetVal;
}

void InputDevice::DeleteTouchPointFromArray(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot)//���������Ϣ�Ӷ�㴥�ص�������ɾ��
{
	if (-1 == pSlot->iTrackingId)//-1��ʾ�õ������һ����ָ̧�𣬹�ֱ�ӷ���false
	{
		return;
	}

	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if (pSlot->iTrackingId == TouchInfoArray[i].iTrackingId)
		{
			TouchInfoArray[i].Clear();
		}
	}
}

int InputDevice::GetTouchPointNum(STouchPointInfo TouchInfoArray[])	//��ȡ��ǰ���µ���ָ����Ŀ
{
	int iNum = 0;
	for (int i = 0; i < MAX_SLOT_NUMBER; i++)
	{
		if (-1 != TouchInfoArray[i].iTrackingId)
		{
			iNum++;
		}
	}
	return iNum;
}

void InputDevice::UnitTest()
{
	InputDevice::GetInstance().ReadKeyboard(true);
}

void InputDevice::SetProtoType(int iProtoType)
{
	m_ProtoType = iProtoType;
	LOGI("SetProtoType m_ProtoType=%d", m_ProtoType);
}

void InputDevice::WriteInitData(int iX, int iY)
{
	auto iPointerFD = m_pAllInputFD[1 + m_iKeyboardFDNumber].fd;

	WriteEvent(iPointerFD, EV_SYN, SYN_MT_REPORT, 0);
	WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);

	WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_X, iX);
	WriteEvent(iPointerFD, EV_ABS, ABS_MT_POSITION_Y, iY);
	WriteEvent(iPointerFD, EV_SYN, SYN_MT_REPORT, 0);
	WriteEvent(iPointerFD, EV_SYN, SYN_REPORT, 0);

}