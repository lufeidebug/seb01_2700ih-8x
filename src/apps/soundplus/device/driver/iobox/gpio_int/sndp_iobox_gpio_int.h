
#ifndef __SNDP_IOBOX_GPIO_INT_H__
#define __SNDP_IOBOX_GPIO_INT_H__

#if defined(__SNDP_IOBOX_GPIO_INT__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SNDP_IOBOX_GPIO_TRACE__

#ifdef __SNDP_IOBOX_GPIO_TRACE__
#define IOB_GPIO_TRACE(num, str, ...)   SNDP_TRACE(1 + num, "[IOB_GPIO] %s, " str, __func__, ##__VA_ARGS__)
#else
#define IOB_GPIO_TRACE(num, str, ...)
#endif


extern const sndp_hal_iobox_s sndp_hal_iobox_gpio_int;

#ifdef __cplusplus
}
#endif

#endif  //__SNDP_IOBOX_GPIO_INT__
#endif  //__SNDP_IOBOX_GPIO_INT_H__


