/* Vanilla OMF resource selection: no external modification packages loaded.
 * Network creation reports unsupported until Aster exposes UDP bind/listen. */
#include "resources/modmanager.h"
#include "utils/log.h"
#include "utils/png_reader.h"
#include "utils/png_writer.h"
#include <enet/enet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
bool modmanager_init(void){return true;}void modmanager_shutdown(void){}void modmanager_set_allowed(bool v){}
bool modmanager_get_bk_background(str *n,sd_vga_image **v){return false;}
bool modmanager_get_sprite(animation_source s,str *n,int a,int f,sd_sprite **v){return false;}
unsigned modmanager_count_music(str *n){return 0;}
bool modmanager_get_music(str *n,unsigned i,unsigned char **v,size_t *len){return false;}
bool modmanager_get_hitcoords(animation_source s,str *n,int a,int f,vector *c,vec2i *o,vec2i *so){return false;}
bool modmanager_get_af_move(str *n,int id,af_move *m){return false;}
bool modmanager_get_bk_animation(str *n,int id,bk_info *m){return false;}
bool modmanager_get_fighter_header(str *n,af *m){return false;}
bool modmanager_get_tournament_mod(const char *n,sd_tournament_file *m){return false;}
bool modmanager_parse_photo_mod(const char *n,sd_pic_photo *m){return false;}
bool modmanager_get_player_pics(sd_pic_file *m){return false;}
bool read_paletted_png(const path *f,unsigned char *b){log_warn("External PNG resources not enabled");return false;}
bool read_paletted_png_from_memory(const unsigned char *b,size_t n,unsigned char *out,int *w,int *h,bool t,vga_palette *p){return false;}
bool write_rgb_png(const path *f,int w,int h,const unsigned char *b,bool a,bool flip){return false;}
bool write_paletted_png(const path *f,int w,int h,const vga_palette *p,const unsigned char *b){return false;}
ENetHost *enet_host_create(const ENetAddress *a,size_t p,size_t c,enet_uint32 i,enet_uint32 o){log_warn("Online mode unavailable: Aster has no UDP bind/listen API");return NULL;}
void enet_host_destroy(ENetHost *h){}
ENetPeer *enet_host_connect(ENetHost *h,const ENetAddress *a,size_t c,enet_uint32 d){return NULL;}
int enet_host_service(ENetHost *h,ENetEvent *e,enet_uint32 timeout){return -1;}
void enet_host_flush(ENetHost *h){}
int enet_peer_send(ENetPeer *p,enet_uint8 c,ENetPacket *v){return -1;}
void enet_peer_disconnect_later(ENetPeer *p,enet_uint32 d){}void enet_peer_reset(ENetPeer *p){}void enet_peer_timeout(ENetPeer *p,enet_uint32 a,enet_uint32 b,enet_uint32 c){}void enet_peer_ping_interval(ENetPeer *p,enet_uint32 ms){}
int enet_socket_set_option(ENetSocket s,ENetSocketOption o,int v){return -1;}
int enet_address_set_host(ENetAddress *a,const char *s){unsigned i[4];char tail;if(sscanf(s,"%u.%u.%u.%u%c",&i[0],&i[1],&i[2],&i[3],&tail)!=4)return -1;for(int j=0;j<4;j++)if(i[j]>255)return -1;a->host=i[0]|(i[1]<<8)|(i[2]<<16)|(i[3]<<24);return 0;}
ENetPacket *enet_packet_create(const void *b,size_t n,enet_uint32 flags){ENetPacket *p=calloc(1,sizeof(*p));if(!p)return NULL;p->flags=flags;p->dataLength=n;if(flags&ENET_PACKET_FLAG_NO_ALLOCATE)p->data=(enet_uint8*)b;else if(n){p->data=malloc(n);if(!p->data){free(p);return NULL;}if(b)memcpy(p->data,b,n);}return p;}
void enet_packet_destroy(ENetPacket *p){if(!p)return;if(p->freeCallback)p->freeCallback(p);if(!(p->flags&ENET_PACKET_FLAG_NO_ALLOCATE))free(p->data);free(p);}
