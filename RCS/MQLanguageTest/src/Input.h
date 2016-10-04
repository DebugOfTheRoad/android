#include <sys/poll.h>
#include <jni.h>

#define MAX_FD_NUMBER	8
#define MAX_SLOT_NUMBER	32
#define MAX_KEY_NUMBER 256

#define PROTO_TYPE_UNKNOW	-1
#define PROTO_TYPE_A		0
#define PROTO_TYPE_B		1

typedef struct STouchPointInfoS
{
	int iX;
	int iY;
	int iTrackingId;	//��ָ��ID���, -1��ʾ����

	void Clear()
	{
		iX = iY = -1;
		iTrackingId = -1;
	}
}STouchPointInfo;


class InputDevice
{
public:
	static void UnitTest();
	static InputDevice& GetInstance();
	static void DeleteInstance();

	int ReadInput(bool bGetKeyDownOnly, bool bIsIncludeTouchScreenDevice, int ReturnValues[]);		//���ڼ�����Ϣ��ֻҪ����ֵ�Ϳ����ˡ����ڴ�������Ϣ������Ҫ��ReturnValues������ø������Ϣ
	int ReadKeyboard(bool bGetKeyDownOnly);
	int WritePointerInput(int iAction, int iX, int iY, int iFingerId, int iTime);
	int Cancel();
	void SetProtoType(int iProtoType);
	void WriteInitData(int iX, int iY);
protected:
	InputDevice();
	~InputDevice();

	int AddDevice(int fd);
	void BuildScanCodeMapping();
	int WriteEvent(int fd, int iType, int iCode, int iValue);

	static bool containsNonZeroByte(const uint8_t* array, uint32_t startIndex, uint32_t endIndex);

private:
	struct 
	{	
		struct pollfd EventPipe;
		struct pollfd m_KeyboardPollFD[MAX_FD_NUMBER];
		struct pollfd m_TouchScreenPollFD[MAX_FD_NUMBER];
	} m_InputDevFD;

	struct STouchInfo
	{
		struct STouchInfoSlot
		{
			int iX;
			int iY;
			int iTrackingId;
			bool bIsModified;

			void Clear()
			{
				iX = iY = -1;
				iTrackingId = 0;
				bIsModified = false;
			}
		} m_Slot[MAX_SLOT_NUMBER];
		UINT m_nCurrentSlot;

		STouchPointInfo m_LatestTouchInfo[MAX_SLOT_NUMBER];
		STouchPointInfo m_CurTouchInfo[MAX_SLOT_NUMBER];

		STouchInfo() 
		{
			m_nCurrentSlot = 0;
			for(auto i = 0; i < MAX_SLOT_NUMBER; i++)
			{
				m_Slot[i].Clear();
				m_LatestTouchInfo[i].Clear();
				m_CurTouchInfo[i].Clear();
			}
		}
	} *m_pTouchInfo;

	struct pollfd *m_pAllInputFD;

	class Cleaner
	{
	public:
		Cleaner() {}; 
		~Cleaner() 
		{
			InputDevice::DeleteInstance();
		}
	}; 

	int m_iKeyboardFDNumber;
	int m_iTouchScreenFDNumber;
	int m_ThreadEventPipe[2];
	BYTE  m_KeyboardScanCodeMapping[MAX_KEY_NUMBER];

	static InputDevice* s_pInstance;
	int m_ProtoType;	//��ʾinput������Э�����ͣ�-1��ʾδ֪��0��ʾtypeA; 1��ʾtypeB
	STouchPointInfo m_MultiTouchPointInfo[MAX_SLOT_NUMBER];

private:
	void AddPointToTouchInfoArray(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot);//���������Ϣ��ӵ���㴥�ص�������
	void CopyTouchInfoArray(STouchPointInfo TouchInfoArrayFrom[], STouchPointInfo TouchInfoArrayTo[]);//���ƶ�㴥������
	bool IsTouchPointMove(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot);//�жϸô����Ƿ��з����ƶ�
	void ClearTouchInfoArray(STouchPointInfo TouchInfoArray[]);//��ն�㴥�ص�����
	int GetTouchUpPoint(STouchPointInfo TouchInfoArrayCur[], STouchPointInfo TouchInfoArrayLatest[]);//��ȡ��ָ̧��ĵ㣬��û����ָ̧���򷵻�-1
	void DeleteTouchPointFromArray(STouchPointInfo TouchInfoArray[], STouchInfo::STouchInfoSlot *pSlot);//���������Ϣ�Ӷ�㴥�ص�������ɾ��
	int GetTouchPointNum(STouchPointInfo TouchInfoArray[]);	//��ȡ��ǰ���µ���ָ����Ŀ

	void WritePointerInputTypeA(int iPointerFD, int iAction, int iX, int iY, int iFingerId);//Type AЭ��ģʽ��¼�ƻط�
};