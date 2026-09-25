/* Compile the actual coordinator against an IDF-shaped host with real pthread
 * mutexes. Faults reproduce IDF 5.5.4's ignored deinit callback semantics. */
#include "board/guition_sdmmc.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static bool initialized, slots[2];
static int ldo, force_deinit, freed_cards, vfs_unmount_calls, init_calls;
static int slot_fail=-1, release_fail=-1, mount_fail, power_fail;
static atomic_int transaction_started, finish_transaction;
static atomic_int interrupt_ready;
static bool long_transaction;
static sdmmc_card_t *mounted;
static sdmmc_host_t captured_host;
static const sdmmc_slot_config_t hosted={.clk=18,.cmd=19,.d0=14,.d1=15,.d2=16,.d3=17,.cd=-1,.wp=-1,.width=4};
static const esp_vfs_fat_mount_config_t config={.max_files=3};
TickType_t xTaskGetTickCount(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (TickType_t)(t.tv_sec*1000+t.tv_nsec/1000000);}
void vTaskDelay(TickType_t t){struct timespec n={t/1000,(long)(t%1000)*1000000};nanosleep(&n,NULL);}
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *m){pthread_mutexattr_t a;assert(!pthread_mutexattr_init(&a));assert(!pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE));assert(!pthread_mutex_init(m,&a));pthread_mutexattr_destroy(&a);return m;}
int xSemaphoreTakeRecursive(SemaphoreHandle_t m,TickType_t t){if(t==portMAX_DELAY)return pthread_mutex_lock(m)==0;TickType_t start=xTaskGetTickCount();do {if(!pthread_mutex_trylock(m))return 1;if(xTaskGetTickCount()-start>=t)break;vTaskDelay(1);}while(1);return 0;}
int xSemaphoreGiveRecursive(SemaphoreHandle_t m){return pthread_mutex_unlock(m)==0;}
static void no_transfer(void){assert(!atomic_load(&transaction_started)||atomic_load(&finish_transaction));}
esp_err_t sdmmc_host_init(void){no_transfer();initialized=true;init_calls++;return ESP_OK;}
esp_err_t sdmmc_host_init_slot(int s,const sdmmc_slot_config_t*c){no_transfer();assert(initialized&&!slots[s]);if(s==slot_fail)return ESP_FAIL;if(s==0){assert(c->clk==43&&c->cmd==44&&c->d0==39&&c->d1==40&&c->d2==41&&c->d3==42);assert(c->cd==-1&&c->wp==-1&&c->width==4&&c->flags==0);}slots[s]=true;return ESP_OK;}
esp_err_t sdmmc_host_deinit_slot(int s){no_transfer();if(s==release_fail)return ESP_FAIL;assert(initialized&&slots[s]);slots[s]=false;if(!slots[0]&&!slots[1])initialized=false;return ESP_OK;}
esp_err_t sdmmc_host_deinit(void){no_transfer();assert(!slots[0]&&!slots[1]);force_deinit++;initialized=false;return ESP_OK;}
esp_err_t sdmmc_host_get_state(sdmmc_host_state_t*s){s->host_initialized=initialized;s->num_of_init_slots=slots[0]+slots[1];return ESP_OK;}
esp_err_t sdmmc_host_set_bus_width(int s,size_t w){no_transfer();assert(slots[s]&&w);return ESP_OK;}
size_t sdmmc_host_get_slot_width(int s){assert(slots[s]);return 4;}
esp_err_t sdmmc_host_set_bus_ddr_mode(int s,bool b){no_transfer();assert(slots[s]&&!b);return ESP_OK;}
esp_err_t sdmmc_host_set_card_clk(int s,uint32_t f){no_transfer();assert(slots[s]&&f);return ESP_OK;}
esp_err_t sdmmc_host_set_cclk_always_on(int s,bool b){(void)b;no_transfer();assert(slots[s]);return ESP_OK;}
esp_err_t sdmmc_host_do_transaction(int s,sdmmc_command_t*c){(void)c;assert(slots[s]);if(long_transaction){atomic_store(&transaction_started,1);while(!atomic_load(&finish_transaction))vTaskDelay(1);}return ESP_OK;}
esp_err_t sdmmc_host_get_real_freq(int s,int*f){assert(slots[s]);*f=20000;return ESP_OK;}
esp_err_t sdmmc_host_set_input_delay(int s,sdmmc_delay_phase_t p){(void)p;no_transfer();assert(slots[s]);return ESP_OK;}
bool sdmmc_host_check_buffer_alignment(int s,const void*b,size_t n){(void)b;(void)n;assert(slots[s]);return true;}
esp_err_t sdmmc_host_is_slot_set_to_uhs1(int s,bool*u){assert(slots[s]);*u=false;return ESP_OK;}
esp_err_t sd_pwr_ctrl_new_on_chip_ldo(const sd_pwr_ctrl_ldo_config_t*c,sd_pwr_ctrl_handle_t*h){assert(c->ldo_chan_id==4&&ldo==0);if(power_fail==1)return ESP_FAIL;ldo++;*h=&ldo;return ESP_OK;}
esp_err_t sd_pwr_ctrl_del_on_chip_ldo(sd_pwr_ctrl_handle_t h){assert(h==&ldo&&ldo==1);if(power_fail==2)return ESP_FAIL;ldo--;return ESP_OK;}
esp_err_t esp_vfs_fat_sdmmc_mount(const char*p,const sdmmc_host_t*h,const void*s,const esp_vfs_fat_mount_config_t*c,sdmmc_card_t**out){(void)p;assert(!mounted&&!c->format_if_mount_failed);assert(h->slot==0&&h->command_timeout_ms==1000&&h->max_freq_khz==20000);assert(h->pwr_ctrl_handle==&ldo);captured_host=*h;if(mount_fail==1)return ESP_ERR_NO_MEM;sdmmc_card_t*card=calloc(1,sizeof(*card));assert(card);card->host=*h;esp_err_t err=h->init();if(err)goto fail;err=sdmmc_host_init_slot(0,s);if(err)goto cleanup;err=h->set_bus_width(0,4);assert(!err);err=h->set_card_clk(0,20000);assert(!err);if(mount_fail==2){err=ESP_FAIL;goto cleanup;}mounted=card;*out=card;return ESP_OK;
cleanup: h->deinit_p(0); /* Deliberately ignore error, exactly like IDF. */
fail:free(card);freed_cards++;return err;}
esp_err_t esp_vfs_fat_sdcard_unmount(const char*p,sdmmc_card_t*c){(void)p;vfs_unmount_calls++;assert(c==mounted);c->host.deinit_p(0); /* IDF ignores this result. */
free(c);freed_cards++;mounted=NULL;return mount_fail==3?ESP_FAIL:ESP_OK;}
static sdmmc_card_t *card_mount(void){sdmmc_card_t*c=NULL;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_OK&&c);return c;}
static void end_card(sdmmc_card_t*c){assert(guition_sdmmc_unmount("/sdcard",c)==ESP_OK);assert(!guition_sdmmc_is_mounted(c));}
static void *card_transfer(void*unused){(void)unused;sdmmc_command_t cmd;assert(captured_host.do_transaction(0,&cmd)==ESP_OK);return NULL;}
static void *hosted_transfer(void*unused){(void)unused;assert(!guition_sdmmc_lock(portMAX_DELAY));sdmmc_command_t cmd;assert(!sdmmc_host_do_transaction(1,&cmd));guition_sdmmc_unlock();return NULL;}
/* The Python runner compiles this public wait function directly from the
 * verified patched source, not a test reimplementation of its loop. */
extern int hosted_sdio_wait_slave_intr(void*,uint32_t);
int fake_wait_for_interrupt(void*ctx,uint32_t ticks){assert(ctx&&ticks==0);return atomic_load(&interrupt_ready)?ESP_OK:ESP_ERR_TIMEOUT;}
static void *interrupt_waiter(void*unused){(void)unused;assert(hosted_sdio_wait_slave_intr((void*)1,portMAX_DELAY)==ESP_OK);return NULL;}
int main(int argc,char**argv){assert(argc==2);const char*n=argv[1];
if(!strcmp(n,"both_orders")){for(int i=0;i<2;i++){sdmmc_card_t*c;if(i==0){assert(!guition_sdmmc_hosted_acquire(1,&hosted));c=card_mount();}else{c=card_mount();assert(!guition_sdmmc_hosted_acquire(1,&hosted));}assert(slots[0]&&slots[1]);if(i==0){assert(!guition_sdmmc_hosted_release());assert(slots[0]&&initialized);end_card(c);}else{end_card(c);assert(slots[1]&&initialized);assert(!guition_sdmmc_hosted_release());}assert(!initialized&&!ldo&&!force_deinit);}}
else if(!strcmp(n,"absent_retry")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));sdmmc_card_t*c=(void*)1;mount_fail=2;for(int i=0;i<4;i++){assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL&&c==NULL);assert(slots[1]&&!slots[0]&&ldo==0);}mount_fail=0;c=card_mount();end_card(c);assert(!guition_sdmmc_hosted_release());assert(freed_cards==5&&!force_deinit);}
else if(!strcmp(n,"slot_failure_quarantine")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));slot_fail=0;sdmmc_card_t*c;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL);assert(slots[1]&&initialized&&!force_deinit);slot_fail=-1;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_ERR_INVALID_STATE);assert(!guition_sdmmc_hosted_release());c=card_mount();end_card(c);}
else if(!strcmp(n,"hosted_failure_card_live")){sdmmc_card_t*c=card_mount();slot_fail=1;assert(guition_sdmmc_hosted_acquire(1,&hosted)==ESP_FAIL);assert(slots[0]&&ldo==1&&!force_deinit);sdmmc_command_t cmd;assert(!c->host.do_transaction(0,&cmd));end_card(c);slot_fail=-1;assert(!guition_sdmmc_hosted_acquire(1,&hosted));assert(!guition_sdmmc_hosted_release());}
else if(!strcmp(n,"no_slots_failure")){slot_fail=0;sdmmc_card_t*c;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL);assert(!initialized&&!ldo&&force_deinit==1);slot_fail=-1;c=card_mount();end_card(c);}
else if(!strcmp(n,"ignored_unmount_deinit_error")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));sdmmc_card_t*c=card_mount();release_fail=0;assert(guition_sdmmc_unmount("/sdcard",c)==ESP_FAIL);assert(freed_cards==1&&!guition_sdmmc_is_mounted(c)&&ldo==1&&slots[0]&&slots[1]);assert(guition_sdmmc_unmount("/sdcard",c)==ESP_ERR_INVALID_STATE&&vfs_unmount_calls==1);assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_ERR_INVALID_STATE);assert(!guition_sdmmc_hosted_release());assert(slots[0]&&!slots[1]&&initialized&&!force_deinit);}
else if(!strcmp(n,"ignored_failed_mount_deinit_error")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));release_fail=0;mount_fail=2;sdmmc_card_t*c;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL&&c==NULL);assert(freed_cards==1&&ldo==1&&slots[0]&&slots[1]);assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_ERR_INVALID_STATE);assert(!force_deinit);}
else if(!strcmp(n,"unregister_error_consumes_card")){sdmmc_card_t*c=card_mount();mount_fail=3;assert(guition_sdmmc_unmount("/sdcard",c)==ESP_FAIL);assert(!guition_sdmmc_is_mounted(c)&&!initialized&&!ldo&&freed_cards==1);}
else if(!strcmp(n,"ldo_cleanup_failure")){sdmmc_card_t*c=card_mount();power_fail=2;assert(guition_sdmmc_unmount("/sdcard",c)==ESP_FAIL&&!guition_sdmmc_is_mounted(c)&&ldo==1);assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL);power_fail=0;c=card_mount();end_card(c);}
else if(!strcmp(n,"early_allocation_failure")){mount_fail=1;sdmmc_card_t*c;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_ERR_NO_MEM&&c==NULL);assert(!ldo&&!initialized&&!init_calls);mount_fail=0;power_fail=1;assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_FAIL&&c==NULL);assert(!ldo&&!initialized);}
else if(!strcmp(n,"configuration_and_duplicate_guards")){sdmmc_slot_config_t bad=hosted;bad.clk=43;assert(guition_sdmmc_hosted_acquire(1,&bad)==ESP_ERR_INVALID_ARG);assert(guition_sdmmc_hosted_acquire(0,&hosted)==ESP_ERR_INVALID_ARG);esp_vfs_fat_mount_config_t format=config;format.format_if_mount_failed=true;sdmmc_card_t*c;assert(guition_sdmmc_mount("/sdcard",&format,&c)==ESP_ERR_INVALID_ARG);assert(!init_calls);assert(!guition_sdmmc_hosted_acquire(1,&hosted));assert(guition_sdmmc_hosted_acquire(1,&hosted)==ESP_ERR_INVALID_STATE);assert(!guition_sdmmc_hosted_release());assert(guition_sdmmc_hosted_release()==ESP_ERR_INVALID_STATE);}
else if(!strcmp(n,"transfer_vs_teardown")){sdmmc_card_t*c=card_mount();assert(!guition_sdmmc_hosted_acquire(1,&hosted));pthread_t thread;long_transaction=true;assert(!pthread_create(&thread,NULL,card_transfer,NULL));while(!atomic_load(&transaction_started))vTaskDelay(1);TickType_t t=xTaskGetTickCount();assert(guition_sdmmc_hosted_release()==ESP_ERR_TIMEOUT);assert(xTaskGetTickCount()-t<600&&slots[0]&&slots[1]);atomic_store(&finish_transaction,1);pthread_join(thread,NULL);assert(!guition_sdmmc_hosted_release());end_card(c);}
else if(!strcmp(n,"hosted_transfer_vs_mount")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));pthread_t thread;long_transaction=true;assert(!pthread_create(&thread,NULL,hosted_transfer,NULL));while(!atomic_load(&transaction_started))vTaskDelay(1);sdmmc_card_t*c;TickType_t t=xTaskGetTickCount();assert(guition_sdmmc_mount("/sdcard",&config,&c)==ESP_ERR_TIMEOUT&&c==NULL);assert(xTaskGetTickCount()-t<600&&slots[1]&&!slots[0]&&!ldo);atomic_store(&finish_transaction,1);pthread_join(thread,NULL);c=card_mount();end_card(c);assert(!guition_sdmmc_hosted_release());}
else if(!strcmp(n,"interrupt_wait_releases_guard")){assert(hosted_sdio_wait_slave_intr((void*)1,0)==ESP_ERR_TIMEOUT);TickType_t start=xTaskGetTickCount();assert(hosted_sdio_wait_slave_intr((void*)1,3)==ESP_ERR_TIMEOUT&&xTaskGetTickCount()-start>=3);assert(!guition_sdmmc_hosted_acquire(1,&hosted));pthread_t thread;assert(!pthread_create(&thread,NULL,interrupt_waiter,NULL));vTaskDelay(3);for(int i=0;i<3;i++){sdmmc_card_t*c=card_mount();end_card(c);}atomic_store(&interrupt_ready,1);pthread_join(thread,NULL);assert(!guition_sdmmc_hosted_release());}
else if(!strcmp(n,"cancel_waiter_under_teardown_guard")){assert(!guition_sdmmc_hosted_acquire(1,&hosted));pthread_t thread;assert(!pthread_create(&thread,NULL,interrupt_waiter,NULL));vTaskDelay(3);assert(!guition_sdmmc_lock(portMAX_DELAY));pthread_cancel(thread);assert(!guition_sdmmc_hosted_release());guition_sdmmc_unlock();pthread_join(thread,NULL);sdmmc_card_t*c=card_mount();end_card(c);}
else assert(!"unknown case");
printf("PASS %s\n",n);return 0;}
