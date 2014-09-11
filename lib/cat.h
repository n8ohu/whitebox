#ifndef __WHITEBOX_CAT_H__
#define __WHITEBOX_CAT_H__

int cat_init(struct whitebox *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt);

int cat_ctl(struct whitebox *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt);

#endif /* __WHITEBOX_CAT_H__ */
