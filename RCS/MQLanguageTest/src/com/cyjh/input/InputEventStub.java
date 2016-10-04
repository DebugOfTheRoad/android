package com.cyjh.input;



public class InputEventStub
{
		//�����豸��type���ͣ�PROTO_TYPE_AΪ0��PROTO_TYPE_BΪ1
		public native void SetProtoType(int iProtoType);
		public native void WriteInitData(int iX, int iY);
		
    public native void TouchDownEvent(int iX, int iY, int iFingerId);
    public native void TouchMoveEvent(int iX, int iY, int iFingerId, int iTime);
    public native void TouchUpEvent(int iFingerId);
    

    static 
    {
    	//System.load(ClientService.libPath);
    }
}
