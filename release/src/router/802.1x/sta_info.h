#ifndef STA_INFO_H
#define STA_INFO_H

struct sta_info* Ap_get_sta(rtapd *apd, u8 *sa, u8 *apidx, u16 ethertype, int sock);
struct sta_info* Ap_get_sta_radius_identifier(rtapd *apd, u8 radius_identifier);
void Ap_sta_hash_add(rtapd *apd, struct sta_info *sta);
void Ap_free_sta(rtapd *apd, struct sta_info *sta);
void Apd_free_stas(rtapd *apd);
void Ap_sta_session_timeout(rtapd *apd, struct sta_info *sta, u32 session_timeout);
void Ap_sta_no_session_timeout(rtapd *apd, struct sta_info *sta);

#endif /* STA_INFO_H */
