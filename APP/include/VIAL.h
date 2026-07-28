/*
 * VIAL.h
 *
 *  Created on: 2025年1月7日
 *      Author: OWNER
 */

#ifndef MINE_VIAL_H_
#define MINE_VIAL_H_

extern UINT8 vial_init(void);  //初始化，如果校验不通过则键盘不支持vial且不能读取配列
extern UINT8 FLASH_DATA_KEY(uint8_t *P_buf);  //从flash读取配列
extern UINT8 FLASH_DATA_VIAL(uint32_t add,uint8_t *P_buf);  //从flash读取vial支持数据
extern UINT8 FLASH_DATA_VIAL_WITE_mode(uint8_t *key);  //写入flash运行方式
extern UINT8 FLASH_DATA_VIAL_WITE_key(uint32_t key_add,uint8_t *p_buf,uint8_t Key_length);//改键支持


#endif /* MINE_VIAL_H_ */
