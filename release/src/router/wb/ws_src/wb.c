
#include <wb.h>					// include the header of web service 
#include <wb_util.h>
#include <curl_api.h>
//#include <wb_test_profile.h>
#include <parse_xml.h>
#include <openssl/md5.h>
#include <ssl_api.h>
#include <log.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#if 0
GetServiceArea 	gs_response;
Login			lg_response;
QueryFriend		qf_response;
#endif

#define get_append_data(...) make_str(__VA_ARGS__)
int 	proc_getservicearea_xml(const char* name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{		
	if(!name || !value || !data_struct) return -1;
	//printf("name <%s> ,value <%s>\n", name, value);
	GetServiceArea* pgsa = (GetServiceArea*)data_struct;	
	#define TEMP_SIZE 128
	//char 	valuetmp[TEMP_SIZE] ={0};
	//memcpy(valuetmp,"value", value );	

	if(!strcmp(name, "status")) {		
		memset(pgsa->status, 0, sizeof(pgsa->status));		
		strcpy(pgsa->status, value);
	//	memcpy(pgsa->status, value, strlen(valuetmp));	
	}else if(!strcmp(name, "servicearea")){			
		memset(pgsa->servicearea, 0, sizeof(pgsa->servicearea));
		strcpy(pgsa->servicearea,value);
	//	memcpy(pgsa->servicearea,value, strlen(valuetmp));		
	}else if(!strcmp(name, "time")){
		memset(pgsa->time, 0, sizeof(pgsa->time));
		strcpy(pgsa->time, value);
	//	memcpy(pgsa->time, value, strlen(valuetmp));		
	}else {
		// error 		
		Cdbg(WB_DBG,"error: unknow tag <%s>, unkonow value <%s>", name, value);
	}
	return 0;
}
//
// charles test variable cnt
//int cnt=0;
//
int	set_srv_name(const char* ip, const char* srv_type, Login* login)
{
	SrvInfo *p =NULL ;

	if(!strcmp(srv_type, "relayinfo"))
		p = login->relayinfoList;
	if(!strcmp(srv_type, "stuninfo"))
		p = login->stuninfoList;
	if(!strcmp(srv_type, "turninfo"))
		p = login->turninfoList;
	Cdbg(WB_DBG,"ip=%s, srv_type=%s, p =%p, login=%p", ip, srv_type, p, login);
	Cdbg(WB_DBG, "ri =%p, si=%p, ti=%p", login->relayinfoList, login->stuninfoList, login->turninfoList);
	if(!p){
		p = (SrvInfo*)malloc(sizeof(SrvInfo)); memset(p, 0 , sizeof(SrvInfo));
		Cdbg(WB_DBG, "p =%p, p->next=%p", p, p->next);
		strcpy(p->srv_ip , ip);
		if(!strcmp(srv_type, "relayinfo"))
			login->relayinfoList = p;
		if(!strcmp(srv_type, "stuninfo"))
			login->stuninfoList = p;
		if(!strcmp(srv_type, "turninfo"))
			login->turninfoList = p;
	}else{
		while(p){
			Cdbg(WB_DBG, "p =%p, p->next=%p", p , p->next);
			if(!p->next) {
				p->next =(SrvInfo*) malloc(sizeof(SrvInfo));
				memset(p->next, 0, sizeof(SrvInfo));
				strcpy(p->next->srv_ip,ip);
				break;
			}
			p= p->next;
		}
	}
	return 0;
}

int 	proc_login_xml(const char* name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	if(!name || !value || !data_struct) return -1;
	
	Login* login = (Login*)data_struct;	
	if(!strcmp(name, "status")) {			
		memset(login->status, 0, sizeof(login->status));
		strcpy(login->status, value);		
	}else if(!strcmp(name, "usersvclevel")) {			
		memset(login->usersvclevel, 0, sizeof(login->usersvclevel));
		strcpy(login->usersvclevel, value);		
	}else if(!strcmp(name, "cusid")) {			
		memset(login->cusid, 0, sizeof(login->cusid));
		strcpy(login->cusid, value);		
	}else if(!strcmp(name, "userticket")) {			
		memset(login->userticket, 0, sizeof(login->userticket));
		strcpy(login->userticket, value);		
	}else if(!strcmp(name, "ssoflag")) {			
		memset(login->ssoflag, 0, sizeof(login->ssoflag));
		strcpy(login->ssoflag, value);		
	}else if(!strcmp(name, "usernickname")) {				
		memset(login->usernickname, 0, sizeof(login->usernickname));
		strcpy(login->usernickname, value);		
	}else if(!strcmp(name, "deviceid")) {		
		
		memset(login->deviceid, 0, sizeof(login->deviceid));
		strcpy(login->deviceid, value);		
	}else if(!strcmp(name, "deviceticket")) {	
		memset(login->deviceticket, 0, sizeof(login->deviceticket));
		strcpy(login->deviceticket, value);		
	}else if(!strcmp(name, "relayinfo")) {				
		set_srv_name(value, name, login);
	}else if(!strcmp(name, "stuninfo")) {				
		set_srv_name(value, name, login);
	}else if(!strcmp(name, "turninfo")) {			
		set_srv_name(value, name, login);
#if 0	   
//		Cdbg(WB_DBG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! turn=%s",value);
		memset(login->turninfo, 0, sizeof(login->turninfo));
		strcpy(login->turninfo, value);		
#endif
	}else if(!strcmp(name, "deviceticketexpiretime")) {				
		memset(login->deviceticketexpiretime, 0, sizeof(login->deviceticketexpiretime));
		strcpy(login->deviceticketexpiretime, value);		
	}else if(!strcmp(name, "time")) {				
		memset(login->time, 0, sizeof(login->time));
		strcpy(login->time, value);		
	}else{
		// else log
		Cdbg(WB_DBG,">>>>>>>>>>>>login error");
	}
	return 0;
}

int proc_set_friend_list(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{
	if(!feild_name || !value || !data_struct) return -1;
	Cdbg(WB_DBG, "****************** feild_name=%s",feild_name );
	Cdbg(WB_DBG, "****************** value=%s",value );
	Cdbg(WB_DBG, "****************** data structure=%p",data_struct );
	
	Friends* pF = (Friends*)data_struct;
#if 0
	if(pQF->Friendlist){
		// find last node of friend
		Friends* p = pQF->FriendList;
		for( ;p;p=p->next){
			Cdbg(WB_DBG, "userid=%s, p=%p", p->userid,p);
		}
	}else{
		// no friend profile save in the list,
		// create first node of list.
		pQF->FriendList = malloc(Friends);
		memset(pQF->pFriends, 0, sizeof(Friends));
	}
#endif
	// no friend profile save in the list,
	// create first node of list.
	size_t len = strlen(value)+1;
	if(!strcmp(feild_name, "userid")){
		memset(pF->userid, 0, len);
		strcpy(pF->userid, value);
	}else if(!strcmp(feild_name, "cusid")){
		memset(pF->cusid, 0, len);
		strcpy(pF->cusid, value);
	}else if(!strcmp(feild_name, "nickname")){
		memset(pF->nickname, 0, len);
		strcpy(pF->nickname, value);
	}else{
		Cdbg(WB_DBG, "unknow tag occurs");
	}
		
	return 0;
}

int proc_queryfriend_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{		
	if(!feild_name || !value || !data_struct) return -1;
	QueryFriend* pQF = (QueryFriend*)data_struct;
	if(!strcmp(feild_name, "friend")) {		
		// go to parse
		//parse_node(xml_buff, wm->ws_storage, wm->ws_storage_size, &proc);	
		PROC_XML_DATA proc =proc_set_friend_list;	  	  	
		size_t fd_struct_size = sizeof(Friends);
		pFriends pF = (pFriends)malloc(fd_struct_size);
		memset(pF, 0, fd_struct_size);
		parse_child_node(xmldata, pF, proc);
		if(pQF->FriendList){
			pFriends p= pQF->FriendList;
			while(p){
				if(!p->next){
					Cdbg(WB_DBG,"break.......");
					p->next = pF;
					break;
				}
				p = p->next;
			}
		}else{
			pQF->FriendList = pF;
		}
	}else if(!strcmp(feild_name, "status")){
		strcpy(pQF->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pQF->time, value);		
	}else{
		Cdbg(WB_DBG,"query friend error!\n");
	}
	return 0;
}

int proc_set_profile_list(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{
	Cdbg(WB_DBG, "start...");
	if(!feild_name || !value || !data_struct) return -1;
	Profile* pF = (Profile*)data_struct;
	size_t len = strlen(value)+1;
	if(!strcmp(feild_name, "deviceid")){
	//	pF->userid = malloc(len);
		memset(pF->deviceid, 0, len);
		strcpy(pF->deviceid, value);
	}else if(!strcmp(feild_name, "devicestatus")){
	//	pF->cusid = malloc(len);
		memset(pF->devicestatus, 0, len);
		strcpy(pF->devicestatus, value);
	}else if(!strcmp(feild_name, "devicename")){
	//	pF->nickname = malloc(len);
		memset(pF->devicename, 0, len);
		strcpy(pF->devicename, value);
	}else if(!strcmp(feild_name, "deviceservice")){
		memset(pF->deviceservice, 0, len);
		strcpy(pF->deviceservice, value);
	}else if(!strcmp(feild_name, "devicenat")){
		memset(pF->devicenat, 0, len);
		strcpy(pF->devicenat, value);
	}else if(!strcmp(feild_name, "devicedesc")){
		memset(pF->devicedesc, 0, len);
		strcpy(pF->devicedesc, value);
	}else{
		Cdbg(WB_DBG, "unknow tag occurs");
	}
	Cdbg(WB_DBG, "end...");
	return 0;
}

int 	proc_listprofile_xml(const char* feild_name,const  char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	Cdbg(WB_DBG, "start... name=%s", feild_name);
	if(!feild_name || !value || !data_struct) return -1;
	ListProfile* lpf = (ListProfile*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(lpf->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(lpf->time, value);		
	}else if(!strcmp(feild_name, "profile")){
		PROC_XML_DATA proc =proc_set_profile_list;	  	  	
		size_t pf_struct_size = sizeof(Profile);
		pProfile pF = (pProfile)malloc(pf_struct_size);
		memset(pF, 0, pf_struct_size);
		parse_child_node(xmldata, pF, proc);
		Cdbg(WB_DBG, "loop profile list");
		if(lpf->pProfileList){
		Cdbg(WB_DBG, " profile list =%p", lpf->pProfileList);
			pProfile p= lpf->pProfileList;
			while(p){
				if(!p->next){
					Cdbg(WB_DBG,"break.......");
					p->next = pF;
					break;
				}
				p = p->next;
			}
		}else{
		Cdbg(WB_DBG, " no profile list ");
			lpf->pProfileList = pF;
		}
	
	}else{
		Cdbg(WB_DBG, "Unknow tag");
	}
	Cdbg(WB_DBG, "end...");
	return 0;
}

int 	proc_updateprofile_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	Cdbg(WB_DBG, "start... name=%s, value=%s", feild_name, value);
	UpdateProfile * pup = (UpdateProfile*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(pup->time, value);		
	}else if(!strcmp(feild_name, "deviceticketexpiretime")){
		strcpy(pup->deviceticketexpiretime, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pup->time, value);		
	}else{

	}
	Cdbg(WB_DBG, "end");
	return 0;
}

int 	proc_logout_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	Logout* plgo = (Logout*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(plgo->time, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(plgo->time, value);		
	}else{

	}
	Cdbg(WB_DBG, "end");
	return 0;
}

int 	proc_getwebpath_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	return 0;
}

int 	proc_createpin_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	CreatePin* pcp = (CreatePin*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(pcp->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pcp->time, value);		
	}else if(!strcmp(feild_name, "pin")){
		strcpy(pcp->pin, value);		
	}else if(!strcmp(feild_name, "deviceticketexpiretime")){
		strcpy(pcp->deviceticketexpiretime, value);		
	}else{

	}
	Cdbg(WB_DBG, "end");

	return 0;
}

int 	proc_querypin_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	QueryPin* pqp = (QueryPin*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(pqp->status, value);		
	}else if(!strcmp(feild_name, "deviceid")){
		strcpy(pqp->deviceid, value);		
	}else if(!strcmp(feild_name, "devicestatus")){
		strcpy(pqp->devicestatus, value);		
	}else if(!strcmp(feild_name, "devicename")){
		strcpy(pqp->devicename, value);		
	}else if(!strcmp(feild_name, "deviceservice")){
		strcpy(pqp->deviceservice, value);		
	}else if(!strcmp(feild_name, "deviceticketexpiretime")){
		strcpy(pqp->deviceticketexpiretime, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pqp->time, value);		
	}else{

	}
	Cdbg(WB_DBG, "end");

	return 0;
}

int 	proc_unregister_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	Cdbg(WB_DBG, "start =========================================================");
	Cdbg(WB_DBG, "name =%s, value=%s", feild_name, value);
	
	UnregisterDevice * pud = (UnregisterDevice*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(pud->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pud->time, value);
	}else{
		Cdbg(WB_DBG, "error");
	}
	Cdbg(WB_DBG, "end <<<<<<<<<<<<<<<<");
	return 0;
}

int 	proc_updateiceinfo_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{	
	Updateiceinfo * pui = (Updateiceinfo*)data_struct;
	
	if(!strcmp(feild_name, "status") ){
		strcpy(pui->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pui->time, value);		
	}else{
	}
	return 0;
}

int 	proc_keepalive_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata)
{
	Keepalive* pka = (Keepalive*)data_struct;
	if(!strcmp(feild_name, "status") ){
		strcpy(pka->status, value);		
	}else if(!strcmp(feild_name, "time")){
		strcpy(pka->time, value);
	}else{
		Cdbg(WB_DBG, "error");
	}
	Cdbg(WB_DBG, "end <<<<<<<<<<<<<<<<");

	return 0;
}

int proc_push_msg_xml(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata){
	Push_Msg *pka = (Push_Msg*)data_struct;

	if(!strcmp(feild_name, "status")){
		strcpy(pka->status, value);
	}
	else if(!strcmp(feild_name, "time")){
		strcpy(pka->time, value);
	}
	else{
		Cdbg(WB_DBG, "error");
	}
	Cdbg(WB_DBG, "end <<<<<<<<<<<<<<<<");

	return 0;
}

PROC_XML_DATA get_proc_fn(WS_ID wi)
{	
	if(wi == e_getservicearea)	return proc_getservicearea_xml;
	if(wi == e_login) 			return proc_login_xml;
	if(wi == e_queryfriend)		return proc_queryfriend_xml;
	if(wi == e_listprofile)		return proc_listprofile_xml;
	if(wi == e_updateprofile)	return proc_updateprofile_xml;
	if(wi == e_logout)			return proc_logout_xml;
	if(wi == e_getwebpath)		return proc_getwebpath_xml;
	if(wi == e_createpin)		return proc_createpin_xml;
	if(wi == e_querypin)			return proc_querypin_xml;
	if(wi == e_unregister)		return proc_unregister_xml;
	if(wi == e_updateiceinfo)	return proc_updateiceinfo_xml;
	if(wi == e_keepalive)		return proc_keepalive_xml;
	if(wi == e_pushsendmsg)		return proc_push_msg_xml;
}

//extend the function for future utility
size_t get_storage_size(WS_ID wi)
{	
	if(wi ==e_getservicearea)	return sizeof(GetServiceArea);
	if(wi ==e_login) 		return sizeof(Login);
	if(wi ==e_queryfriend)		return sizeof(QueryFriend);
	if(wi ==e_listprofile)		return sizeof(ListProfile);
	if(wi ==e_updateprofile)	return sizeof(UpdateProfile);
	if(wi ==e_logout)		return sizeof(Logout);
	if(wi ==e_getwebpath)		return sizeof(Getwebpath);
	if(wi ==e_createpin)		return sizeof(CreatePin);
	if(wi ==e_querypin)		return sizeof(QueryPin);
	if(wi ==e_unregister)		return sizeof(UnregisterDevice);
	if(wi ==e_updateiceinfo)	return sizeof(updateiceinfo_in);
	return 0;
}

//extend the function for future utility
void* get_response_storage(WS_ID wi)
{
	void* storage = NULL;
	if(wi ==e_getservicearea)	storage = (void*)malloc(sizeof(GetServiceArea));
	if(wi ==e_login) 			storage = (void*)malloc(sizeof(Login));
	if(wi ==e_queryfriend)		storage = (void*)malloc(sizeof(QueryFriend));
	if(wi ==e_listprofile)		storage = (void*)malloc(sizeof(ListProfile));
	if(wi ==e_updateprofile)	storage = (void*)malloc(sizeof(UpdateProfile));
	if(wi ==e_logout)			storage = (void*)malloc(sizeof(Logout));
	if(wi ==e_getwebpath)		storage = (void*)malloc(sizeof(Getwebpath));
	if(wi ==e_createpin)		storage = (void*)malloc(sizeof(CreatePin));
	if(wi ==e_querypin)			storage = (void*)malloc(sizeof(QueryPin));
	if(wi ==e_unregister)		storage = (void*)malloc(sizeof(UnregisterDevice));
	if(wi ==e_updateiceinfo)	storage = (void*)malloc(sizeof(Updateiceinfo));
		
	return storage;
}

int process_xml(char* xml_buff, WS_MANAGER*	wm)
{
	//printf("resp buff  >>>>>>>>>>>>>>>>>>> %s\n", xml_buff);  	
	PROC_XML_DATA proc = NULL;	  	  	
	proc = get_proc_fn(wm->ws_id);
	//wm->ws_storage 			= get_storage(wm->ws_id);
	//wm->ws_storage_size 	= get_storage_size(wm->ws_id);
	Cdbg(WB_DBG, "go parse node");		
	parse_node(xml_buff, wm->ws_storage, wm->ws_storage_size, proc);
}

/* curl	write callback,	write to libxml2 text buffer  */ 
size_t write_cb(char *in, size_t size, size_t nmemb, void* cb_data)
{
	size_t 			r	= size * nmemb;  
	RWCB* rwcb = (RWCB*)cb_data;
	char* buff = (char*)rwcb->write_data;
	if(buff){
		Cdbg(WB_DBG, " new buff data >>>>>>>>>>>>>>>>\n %s\n, r =%d, sizeIn =%d", in, r, strlen(in));
		size_t cb_org_size = strlen(buff);
		size_t total_size = cb_org_size +r+1;
		Cdbg(WB_DBG, "....0, total_size=%d", total_size);
		char* new_cb_data = (char*)malloc(total_size);
		Cdbg(WB_DBG, "....1");
		memset(new_cb_data, 0, total_size);
		Cdbg(WB_DBG, "....2");
		strcpy(new_cb_data, buff);
		// fix curl bug of tail, extra characters occur in the end
		//void* tmp_addr = new_cb_data+strlen(buff);
		//char* tmp = (char*)tmp_addr;
		//strncpy(tmp, in, r);
		Cdbg(WB_DBG, "....3");
		strncat(new_cb_data, in, r);
		Cdbg(WB_DBG, "....44");
		if(buff) free(buff);
		buff = new_cb_data;
	}else{
		buff = (char*)malloc(r+1);
		memset(buff, 0, r+1);
		strcpy(buff, in );
	}
	rwcb->write_data = buff;
	return(r);
}

static size_t read_callback(void *ptr, size_t size,	size_t nmemb, void *userp)
{
  struct input_info* pooh = (struct input_info*)userp;

  if(size*nmemb	< 1)
	return 0;

  if(pooh->sizeleft) {
	*(char *)ptr = pooh->readptr[0]; /*	copy one single	byte */	
	pooh->readptr++;				 /*	advance	pointer	*/
	pooh->sizeleft--;				 /*	less data left */
	return 1;						 /*	we return 1	byte at	a time!	*/
  }

  return 0;							 /*	no more	data left to deliver */
}

void get_wm(WS_MANAGER* wsM, WS_ID wsID, void* pSrvType, size_t SrvSize )
{
	memset(wsM, 0 , sizeof(wsM));
      	wsM->ws_id                = wsID;
        wsM->ws_storage           = pSrvType;
        wsM->ws_storage_size      = SrvSize;
}

int send_req(char* url, char* append_data, char** response_data )
{
	const char* custom_head[] = {wb_custom_header, NULL};

	Cdbg(WB_DBG, "start");
	struct input_info 	inbuf;
	inbuf.readptr = append_data;
	inbuf.sizeleft= strlen(append_data);

	RWCB rwcb;
	memset(&rwcb, 0, sizeof(rwcb));	
	rwcb.write_cb 	= &write_cb;
	rwcb.read_cb 	= &read_callback;
	rwcb.pInput 	= &inbuf;

	curl_io(url, custom_head, &rwcb);
	size_t resp_len = strlen((char*)rwcb.write_data)+1;
	*response_data = (char*)malloc(resp_len); memset(*response_data, 0, resp_len); 
	strcpy(*response_data, (const char*)rwcb.write_data);
	if(rwcb.write_data) free(rwcb.write_data);
	return 0;
}

int send_keepalive_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	Keepalive*	pka
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, KEEP_ALIVE );
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(keepalive_template, cusid, deviceid, deviceticket, pka);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_keepalive, pka, sizeof(Keepalive));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);

	return 0;

}

int send_updateiceinfo_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	const char*	iceinfo,
	Updateiceinfo*	pui
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, UPDATE_ICE_INFO);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(updateiceinfo_template, cusid, deviceid, deviceticket, iceinfo);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_updateiceinfo, pui, sizeof(Updateiceinfo));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);

	return 0;
}

int send_unregister_device_req(
	const char* 	server,
	const char* 	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	UnregisterDevice*	pud
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, UNREGISTER);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(unregister_template, cusid, deviceid, deviceticket);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_unregister, pud, sizeof(UnregisterDevice));
	process_xml(xml_outbuf, &ws_manager);
	Cdbg(WB_DBG, "process xml done");
	free_webpath(url);

	return 0;
}

int send_query_pin_req(
	const char*	server,
	const char*	cusid,
	const char*	deviceticket,
	const char*	pin,
	QueryPin*	pqp
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, QUERY_PIN);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(querypin_template, cusid,  deviceticket, pin);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_querypin, pqp, sizeof(QueryPin));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);
	return 0;
}

int send_create_pin_req(
	const char* 	server,
	const char*	cusid,
	const char*	deviceid,
	const char*	deviceticket,
	Getwebpath*	pgw
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, CREATE_PIN);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(createpin_template, cusid, deviceid, deviceticket);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_createpin, pgw, sizeof(CreatePin));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);
	return 0;

}

int send_logout_req(
	const char* 	server, 
	const char* 	cusid,
	const char* 	deviceid,
	const char* 	deviceticket,
	Logout*		plgo
)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, LOGOUT);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(logout_template, cusid, deviceid, deviceticket);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_logout, plgo, sizeof(Logout));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);
	return 0;
}

int send_update_profile_req(
	const char* server,
	const char* cusid,
	const char* deviceid,
	const char* deviceticket,
	const char* devicename,
	const char* deviceservice,
	const char* devicestatus,
	const char* permission,
	const char* devicenat,
	const char* devicedesc,
	pUpdateProfile pup)
{
	char*	url = get_webpath(TRANSFER_TYPE, server, UPDATE_PROFILE);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(updateprofile_template, cusid, deviceid,
			deviceticket, devicename, deviceservice, devicestatus, 
			permission, devicenat, devicedesc);
	Cdbg(WB_DBG, "input data >>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_updateprofile, pup, sizeof(UpdateProfile));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);

	return 0;
}

int send_list_profile_req(
	const char* 	server,
	const char*	cusid,
	const char*	userticket,
	const char*	deviceticket,
	const char*	friendid,
	const char*	deviceid,
	pListProfile	p_ListProfile) //out
{
	char*	url = get_webpath(TRANSFER_TYPE, server, LIST_PROFILE);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(listprofile_template, cusid, 
		userticket, deviceticket, friendid, deviceid);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml_outbuf >>>>>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_listprofile, p_ListProfile, sizeof(ListProfile));
	process_xml(xml_outbuf, &ws_manager);
	free_webpath(url);
	return 0;
}

int send_query_friend_req(
	const char* server,
	const char* user_ticket,
	QueryFriend*	fd_list
	)
{
	Cdbg(WB_DBG, "Start Send fd_list=%p", fd_list);
	char* url = get_webpath(TRANSFER_TYPE, server, QUERY_FRIEND);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(queryfriend_template, user_ticket);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
//	send_req(url, append_data,fd_list, sizeof(QueryFriend), e_queryfriend);
	Cdbg(WB_DBG, "Start Send Done, >>>>> Return data >>>>>\n%s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_queryfriend, fd_list, sizeof(QueryFriend));
	process_xml(xml_outbuf, &ws_manager);
	if(xml_outbuf) free(xml_outbuf);
	free_webpath(url);
	Cdbg(WB_DBG,">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>============= fd_list->FriendList=%p", fd_list->FriendList);
	return 0;
}

int send_loginbyticket_req(
	const char* server,
//	const char* userid, 
//	const char* passwd,
	const char* userticket,
	const char* devicemd5mac,
	const char*	devicename,
	const char*	deviceservice,
	const char* devicetype,
	const char*	permission,
	Login*		pLogin
)
{	
	char* url = get_webpath(TRANSFER_TYPE, server, LOGIN);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(loginbyticket_template, 
					userticket, devicemd5mac, devicename,
				deviceservice, devicetype, permission);
	Cdbg(WB_DBG, "append_data >>>>>>>>>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml out buffer >>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_login, pLogin, sizeof(Login));
//	Cdbg(WB_DBG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<< server=%s", server);
	process_xml(xml_outbuf, &ws_manager);
//	Cdbg(WB_DBG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<STUN  SRV=%s", pLogin->stuninfo);
	if(xml_outbuf) free(xml_outbuf);
	free_webpath(url);
//	Cdbg(WB_DBG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<STUN  SRV=%s", pLogin->stuninfo);
	return 0;
}


int send_login_req(
	const char* server,
	const char* userid, 
	const char* passwd,
	const char* devicemd5mac,
	const char*	devicename,
	const char*	deviceservice,
	const char* devicetype,
	const char*	permission,
	Login*		pLogin
)
{	
	char* url = get_webpath(TRANSFER_TYPE, server, LOGIN);
	const char* custom_head[] = {wb_custom_header, NULL};
	char* append_data	= get_append_data(login_template, 
					userid,passwd, devicemd5mac, devicename,
				deviceservice, devicetype, permission);
	Cdbg(WB_DBG, "append_data >>>>>>>>>>>>>>\n %s", append_data);
	char* xml_outbuf=NULL;
	send_req(url, append_data, &xml_outbuf);
	Cdbg(WB_DBG, "xml out buffer >>>>>>>>>>>>>>>>>>>\n %s", xml_outbuf);
	free_append_data(append_data);
	WS_MANAGER ws_manager;
	get_wm(&ws_manager, e_login, pLogin, sizeof(Login));
//	Cdbg(WB_DBG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<< server=%s", server);
	process_xml(xml_outbuf, &ws_manager);
	if(xml_outbuf) free(xml_outbuf);
	free_webpath(url);
	return 0;
}

int send_getservicearea_req(
	const char* server, 
	const char* userid, 
	const char* passwd,
	GetServiceArea* pGSA//out put
	)
{	
	char* url	= get_webpath(TRANSFER_TYPE, server, GET_SERVICE_AREA);
	const char* custom_head[] = {wb_custom_header, NULL};	
	char* append_data	= get_append_data(getservicearea_template, userid, passwd);
	char* xml_outbuf =NULL;
	Cdbg(WB_DBG, "ws manager setting 1, xml_outbuf=%p", xml_outbuf);
	
	send_req(url, append_data, &xml_outbuf);

	Cdbg(WB_DBG, "ws manager setting");
	WS_MANAGER	ws_manager;
	get_wm(&ws_manager, e_getservicearea, pGSA, sizeof(GetServiceArea));
	Cdbg(WB_DBG, "ws manager setting done");
	Cdbg(WB_DBG, "start proc xml");
	process_xml(xml_outbuf, &ws_manager);
	Cdbg(WB_DBG, "end proc xml outbuf >>>>>>>>>>>>\n %s", xml_outbuf);

	free_append_data(append_data);	
	if(xml_outbuf) free(xml_outbuf);
	free_webpath(url);
	return 0;
}

int send_push_msg_req(
	const char *server,
	const char *mac,
	const char *token,
	const char *msg,
	Push_Msg *pPm
	)
{
	char *url	= get_webpath(TRANSFER_TYPE, server, PUSH_SENDMSG);
	char *append_data	= get_append_data(push_msg_template, mac, token, msg);
	char *xml_outbuf = NULL;

	Cdbg(WB_DBG, "send_push_msg_req: send append_data:\n%s.\n", append_data);
	send_req(url, append_data, &xml_outbuf);

	Cdbg(WB_DBG, "send_push_msg_req: ws manager setting.\n");
	WS_MANAGER	ws_manager;
	get_wm(&ws_manager, e_getservicearea, pPm, sizeof(Push_Msg));
	Cdbg(WB_DBG, "send_push_msg_req: done.\n");
	Cdbg(WB_DBG, "send_push_msg_req: start proc xml.\n");
	process_xml(xml_outbuf, &ws_manager);
	Cdbg(WB_DBG, "end proc xml outbuf >>>>>>>>>>>>\n%s", xml_outbuf);

	free_append_data(append_data);	
	if(xml_outbuf) free(xml_outbuf);
	free_webpath(url);
	return 0;
}


#if 0
int main()
{
	printf("main ....\n");
	char* url 	= NULL;
	// get service area ...	
	url	= get_webpath(TRANSFER_TYPE, SERVER,GET_SERVICE_AREA );	
	int test = send_getservicearea_req(url, ASUS_VIP_ID, ASUS_VIP_PWD );		
	free_webpath(url);	
	
	// login
	url = get_webpath(TRANSFER_TYPE, gs_response.servicearea, LOGIN);
	char md5string[MD_STR_LEN] = {0};	
	get_md5_string("00:0c:29:62:72:68", md5string);	
	send_login_req(url, ASUS_VIP_ID, ASUS_VIP_PWD, md5string,"", "","","");
	
	//get friend list
	
	
	free_webpath(url);
	
	
	
	return 0;
}
#endif
