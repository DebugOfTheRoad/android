package com.buxiubianfu.IME.command;

import java.util.Map;

import com.buxiubianfu.IME.command.Data.CommandData;



//ָ��ӿ�
public interface ICommand {
	
			/**
			 * @param cmd �����ָ��
			 * @Intent ���ݵĲ���
			 * @return ���ؽ��
			 */
			String Do(CommandData commandData);
}
