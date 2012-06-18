
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include "rtdot1x.h"
#include "ieee802_1x.h"
#include "eapol_sm.h"
#include "eloop.h"

/* TODO:
 * implement state machines: Controlled Directions and Key Receive
 */


/* EAPOL state machines are described in IEEE Std 802.1X-2001, Chap. 8.5 */

#define setPortAuthorized() \
ieee802_1x_set_sta_authorized(sm->rtapd, sm->sta, 1)
#define setPortUnauthorized() \
ieee802_1x_set_sta_authorized(sm->rtapd, sm->sta, 0)

/* procedures */
#define txCannedFail(x) ieee802_1x_tx_canned_eap(sm->rtapd, sm->sta, (x), 0)
#define txCannedSuccess(x) ieee802_1x_tx_canned_eap(sm->rtapd, sm->sta, (x), 1)
/* TODO: IEEE 802.1aa/D4 replaces txReqId(x) with txInitialMsg(x); value of
 * initialEAPMsg should be used to select which type of EAP packet is sent;
 * Currently, hostapd only supports EAP Request/Identity, so this can be
 * hardcoded. */
#define txInitialMsg(x) ieee802_1x_request_identity(sm->rtapd, sm->sta, (x))
#define txReq(x) ieee802_1x_tx_req(sm->rtapd, sm->sta, (x))
#define sendRespToServer ieee802_1x_send_resp_to_server(sm->rtapd, sm->sta)
/* TODO: check if abortAuth would be needed for something */
#define abortAuth do { } while (0)
#define txKey(x) ieee802_1x_tx_key(sm->rtapd, sm->sta, (x))

/* Definitions for clarifying state machine implementation */
#define SM_STATE(machine, state) \
static void sm_ ## machine ## _ ## state ## _Enter(struct eapol_state_machine \
*sm)

#define SM_ENTRY(machine, _state, _data) \
sm->_data.state = machine ## _ ## _state; \
if (sm->rtapd->conf->debug >= HOSTAPD_DEBUG_MINIMAL) \
	DBGPRINT(RT_DEBUG_ERROR,"IEEE 802.1X: " MACSTR " " #machine " entering state " #_state \
		"\n", MAC2STR(sm->sta->addr));

#define SM_ENTER(machine, state) sm_ ## machine ## _ ## state ## _Enter(sm)

#define SM_STEP(machine) \
static void sm_ ## machine ## _Step(struct eapol_state_machine *sm)

#define SM_STEP_RUN(machine) sm_ ## machine ## _Step(sm)

/* Port Timers state machine - implemented as a function that will be called
 * once a second as a registered event loop timeout */

static void eapol_port_timers_tick(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_state_machine *state = timeout_ctx;

	if (state->aWhile > 0)
		state->aWhile--;
	if (state->quietWhile > 0)
		state->quietWhile--;
	if (state->reAuthWhen > 0)
		state->reAuthWhen--;
	if (state->txWhen > 0)
		state->txWhen--;

	eapol_sm_step(state);

	eloop_register_timeout(1, 0, eapol_port_timers_tick, eloop_ctx, state);
}



/* Authenticator PAE state machine */

SM_STATE(AUTH_PAE, INITIALIZE)
{
	SM_ENTRY(AUTH_PAE, INITIALIZE, auth_pae);
	sm->currentId = 1;
	sm->auth_pae.portMode = Auto;
}


SM_STATE(AUTH_PAE, DISCONNECTED)
{
	int from_initialize = sm->auth_pae.state == AUTH_PAE_INITIALIZE;

	if (sm->auth_pae.state == AUTH_PAE_CONNECTING &&
	    sm->auth_pae.eapLogoff)
		sm->auth_pae.authEapLogoffsWhileConnecting++;

	SM_ENTRY(AUTH_PAE, DISCONNECTED, auth_pae);

	sm->portStatus = Unauthorized;
	setPortUnauthorized();
	sm->auth_pae.eapLogoff = FALSE;
	sm->auth_pae.reAuthCount = 0;
	/* IEEE 802.1X state machine uses txCannedFail() always in this state.
	 * However, sending EAP packet with failure code seems to cause WinXP
	 * Supplicant to deauthenticate, which will set portEnabled = FALSE and
	 * state machines end back to INITIALIZE and then back here to send
	 * canned failure, and so on.. Avoid this by not sending failure packet
	 * when DISCONNECTED state is entered from INITIALIZE state. */
 	if (!from_initialize) {
		txCannedFail(sm->currentId);
		sm->currentId++;
	}
}


SM_STATE(AUTH_PAE, CONNECTING)
{
	if (sm->auth_pae.state != AUTH_PAE_CONNECTING)
		sm->auth_pae.authEntersConnecting++;

	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATED) {
		if (sm->reAuthenticate)
			sm->auth_pae.authAuthReauthsWhileAuthenticated++;
		if (sm->auth_pae.eapStart)
			sm->auth_pae.authAuthEapStartsWhileAuthenticated++;
		if (sm->auth_pae.eapLogoff)
			sm->auth_pae.authAuthEapLogoffWhileAuthenticated++;
	}

	SM_ENTRY(AUTH_PAE, CONNECTING, auth_pae);

	sm->auth_pae.eapStart = FALSE;
	sm->reAuthenticate = FALSE;
	sm->txWhen = sm->auth_pae.txPeriod;
	sm->auth_pae.rxInitialRsp = FALSE;
	txInitialMsg(sm->currentId);
	sm->auth_pae.reAuthCount++;
}


SM_STATE(AUTH_PAE, HELD)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING && sm->authFail)
		sm->auth_pae.authAuthFailWhileAuthenticating++;

	SM_ENTRY(AUTH_PAE, HELD, auth_pae);

	sm->portStatus = Unauthorized;
	setPortUnauthorized();
	sm->quietWhile = sm->auth_pae.quietPeriod;
	sm->auth_pae.eapLogoff = FALSE;
	sm->currentId++;

}


SM_STATE(AUTH_PAE, AUTHENTICATED)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING && sm->authSuccess)
		sm->auth_pae.authAuthSuccessesWhileAuthenticating++;
							
	SM_ENTRY(AUTH_PAE, AUTHENTICATED, auth_pae);

	sm->portStatus = Authorized;
	setPortAuthorized();
	sm->auth_pae.reAuthCount = 0;
	sm->currentId++;
}


SM_STATE(AUTH_PAE, AUTHENTICATING)
{
	if (sm->auth_pae.state == AUTH_PAE_CONNECTING &&
	    sm->auth_pae.rxInitialRsp)
		sm->auth_pae.authEntersAuthenticating++;

	SM_ENTRY(AUTH_PAE, AUTHENTICATING, auth_pae);

	sm->authSuccess = FALSE;
	sm->authFail = FALSE;
	sm->authTimeout = FALSE;
	sm->authStart = TRUE;
}


SM_STATE(AUTH_PAE, ABORTING)
{
	if (sm->auth_pae.state == AUTH_PAE_AUTHENTICATING) {
		if (sm->authTimeout)
			sm->auth_pae.authAuthTimeoutsWhileAuthenticating++;
		if (sm->reAuthenticate)
			sm->auth_pae.authAuthReauthsWhileAuthenticating++;
		if (sm->auth_pae.eapStart)
			sm->auth_pae.authAuthEapStartsWhileAuthenticating++;
		if (sm->auth_pae.eapLogoff)
			sm->auth_pae.authAuthEapLogoffWhileAuthenticating++;
	}

	SM_ENTRY(AUTH_PAE, ABORTING, auth_pae);

	sm->authAbort = TRUE;
	sm->currentId++;
}


SM_STATE(AUTH_PAE, FORCE_AUTH)
{
	SM_ENTRY(AUTH_PAE, FORCE_AUTH, auth_pae);

	sm->portStatus = Authorized;
	setPortAuthorized();
	sm->auth_pae.portMode = ForceAuthorized;
	sm->auth_pae.eapStart = FALSE;
	txCannedSuccess(sm->currentId);
	sm->currentId++;
}


SM_STATE(AUTH_PAE, FORCE_UNAUTH)
{
	SM_ENTRY(AUTH_PAE, FORCE_UNAUTH, auth_pae);

	sm->portStatus = Unauthorized;
	setPortUnauthorized();
	sm->auth_pae.portMode = ForceUnauthorized;
	sm->auth_pae.eapStart = FALSE;
	txCannedFail(sm->currentId);
	sm->currentId++;
}


SM_STEP(AUTH_PAE)
{
	if ((sm->portControl == Auto &&
	     sm->auth_pae.portMode != sm->portControl) ||
	    sm->initialize || !sm->portEnabled)
		SM_ENTER(AUTH_PAE, INITIALIZE);
	else if (sm->portControl == ForceAuthorized &&
		 sm->auth_pae.portMode != sm->portControl &&
		 !(sm->initialize || !sm->portEnabled))
		SM_ENTER(AUTH_PAE, FORCE_AUTH);
	else if (sm->portControl == ForceUnauthorized &&
		 sm->auth_pae.portMode != sm->portControl &&
		 !(sm->initialize || !sm->portEnabled))
		SM_ENTER(AUTH_PAE, FORCE_UNAUTH);
	else {
		switch (sm->auth_pae.state) {
		case AUTH_PAE_INITIALIZE:
			SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_DISCONNECTED:
			SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_HELD:
			if (sm->quietWhile == 0)
				SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_CONNECTING:
			if (sm->auth_pae.eapLogoff ||
			    sm->auth_pae.reAuthCount > sm->auth_pae.reAuthMax)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if (sm->auth_pae.rxInitialRsp &&
				 sm->auth_pae.reAuthCount <=
				 sm->auth_pae.reAuthMax)
				SM_ENTER(AUTH_PAE, AUTHENTICATING);
			else if ((sm->txWhen == 0 || sm->auth_pae.eapStart ||
				  sm->reAuthenticate) &&
				 sm->auth_pae.reAuthCount <=
				 sm->auth_pae.reAuthMax)
				SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_AUTHENTICATED:
			if (sm->auth_pae.eapStart || sm->reAuthenticate)
				SM_ENTER(AUTH_PAE, CONNECTING);
			else if (sm->auth_pae.eapLogoff || !sm->portValid)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			break;
		case AUTH_PAE_AUTHENTICATING:
			if (sm->authSuccess && sm->portValid)
				SM_ENTER(AUTH_PAE, AUTHENTICATED);
			else if (sm->authFail)
				SM_ENTER(AUTH_PAE, HELD);
			else if (sm->reAuthenticate || sm->auth_pae.eapStart ||
				 sm->auth_pae.eapLogoff ||
				 sm->authTimeout)
				SM_ENTER(AUTH_PAE, ABORTING);
			break;
		case AUTH_PAE_ABORTING:
			if (sm->auth_pae.eapLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, DISCONNECTED);
			else if (!sm->auth_pae.eapLogoff && !sm->authAbort)
				SM_ENTER(AUTH_PAE, CONNECTING);
			break;
		case AUTH_PAE_FORCE_AUTH:
			if (sm->auth_pae.eapStart)
				SM_ENTER(AUTH_PAE, FORCE_AUTH);
			break;
		case AUTH_PAE_FORCE_UNAUTH:
			if (sm->auth_pae.eapStart)
				SM_ENTER(AUTH_PAE, FORCE_UNAUTH);
			break;
		}
	}
}



/* Backend Authentication state machine */

SM_STATE(BE_AUTH, INITIALIZE)
{
	SM_ENTRY(BE_AUTH, INITIALIZE, be_auth);

	abortAuth;
	sm->authAbort = FALSE;
}


SM_STATE(BE_AUTH, REQUEST)
{
	SM_ENTRY(BE_AUTH, REQUEST, be_auth);

	sm->currentId = sm->be_auth.idFromServer;
	txReq(sm->currentId);
	sm->be_auth.backendOtherRequestsToSupplicant++;
	sm->aWhile = sm->be_auth.suppTimeout;
	sm->be_auth.reqCount++;
}


SM_STATE(BE_AUTH, RESPONSE)
{
	SM_ENTRY(BE_AUTH, RESPONSE, be_auth);

	sm->be_auth.aReq = sm->be_auth.aSuccess = FALSE;
	sm->authTimeout = FALSE;
	sm->be_auth.rxResp = sm->be_auth.aFail = FALSE;

	sm->aWhile = sm->be_auth.serverTimeout;
	sm->be_auth.reqCount = 0;
	sendRespToServer;
	sm->be_auth.backendResponses++;
}


SM_STATE(BE_AUTH, SUCCESS)
{
	SM_ENTRY(BE_AUTH, SUCCESS, be_auth);

	sm->currentId = sm->be_auth.idFromServer;
	txReq(sm->currentId);
	sm->authSuccess = TRUE;
}


SM_STATE(BE_AUTH, FAIL)
{
	SM_ENTRY(BE_AUTH, FAIL, be_auth);

	sm->currentId = sm->be_auth.idFromServer;
	txReq(sm->currentId);
	sm->authFail = TRUE;
}


SM_STATE(BE_AUTH, TIMEOUT)
{
	SM_ENTRY(BE_AUTH, TIMEOUT, be_auth);

	if (sm->portStatus == Unauthorized)
		txCannedFail(sm->currentId);
	sm->authTimeout = TRUE;
}


SM_STATE(BE_AUTH, IDLE)
{
	SM_ENTRY(BE_AUTH, IDLE, be_auth);

	sm->authStart = FALSE;
	sm->be_auth.reqCount = 0;
}


SM_STEP(BE_AUTH)
{
	if (sm->portControl != Auto || sm->initialize || sm->authAbort) {
		SM_ENTER(BE_AUTH, INITIALIZE);
		return;
	}

	switch (sm->be_auth.state) {
	case BE_AUTH_INITIALIZE:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_REQUEST:
		if (sm->aWhile == 0 &&
		    sm->be_auth.reqCount != sm->be_auth.maxReq)
			SM_ENTER(BE_AUTH, REQUEST);
		else if (sm->be_auth.rxResp)
			SM_ENTER(BE_AUTH, RESPONSE);
		else if (sm->aWhile == 0 &&
			 sm->be_auth.reqCount >= sm->be_auth.maxReq)
			SM_ENTER(BE_AUTH, TIMEOUT);
		break;
	case BE_AUTH_RESPONSE:
		if (sm->be_auth.aReq) {
			sm->be_auth.backendAccessChallenges++;
			SM_ENTER(BE_AUTH, REQUEST);
		} else if (sm->aWhile == 0)
			SM_ENTER(BE_AUTH, TIMEOUT);
		else if (sm->be_auth.aFail) {
			sm->be_auth.backendAuthFails++;
			SM_ENTER(BE_AUTH, FAIL);
		} else if (sm->be_auth.aSuccess) {
			sm->be_auth.backendAuthSuccesses++;
			SM_ENTER(BE_AUTH, SUCCESS);
		}
		break;
	case BE_AUTH_SUCCESS:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_FAIL:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_TIMEOUT:
		SM_ENTER(BE_AUTH, IDLE);
		break;
	case BE_AUTH_IDLE:
		if (sm->authStart)
			SM_ENTER(BE_AUTH, RESPONSE);
		break;
	}
}



/* Reauthentication Timer state machine */

SM_STATE(REAUTH_TIMER, INITIALIZE)
{
	SM_ENTRY(REAUTH_TIMER, INITIALIZE, reauth_timer);
 
	sm->reAuthWhen = sm->reauth_timer.reAuthPeriod;
}


SM_STATE(REAUTH_TIMER, REAUTHENTICATE)
{
	SM_ENTRY(REAUTH_TIMER, REAUTHENTICATE, reauth_timer);
	sm->reAuthenticate = TRUE;
}


SM_STEP(REAUTH_TIMER)
{
	if (sm->portControl != Auto || sm->initialize ||
	    sm->portStatus == Unauthorized ||
	    !sm->reauth_timer.reAuthEnabled) {
		SM_ENTER(REAUTH_TIMER, INITIALIZE);
		return;
	}

	switch (sm->reauth_timer.state) {
	case REAUTH_TIMER_INITIALIZE:
		if ((sm->reAuthWhen == 0)  )
			SM_ENTER(REAUTH_TIMER, REAUTHENTICATE);
		break;
	case REAUTH_TIMER_REAUTHENTICATE:
		SM_ENTER(REAUTH_TIMER, INITIALIZE);
		break;
	}
}

/* Authenticator Key Transmit state machine */
SM_STATE(AUTH_KEY_TX, NO_KEY_TRANSMIT)
{
	DBGPRINT(RT_DEBUG_INFO,"AUTH_KEY_TX, NO_KEY_TRANSMIT\n");
	SM_ENTRY(AUTH_KEY_TX, NO_KEY_TRANSMIT, auth_key_tx);
}

SM_STATE(AUTH_KEY_TX, KEY_TRANSMIT)
{
	DBGPRINT(RT_DEBUG_INFO,"(AUTH_KEY_TX, KEY_TRANSMIT)\n");
	SM_ENTRY(AUTH_KEY_TX, KEY_TRANSMIT, auth_key_tx);

	txKey(sm->currentId);
	sm->keyAvailable = FALSE;
}

SM_STEP(AUTH_KEY_TX)
{
	if (sm->initialize || sm->portControl != Auto) {
		SM_ENTER(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		return;
	}

	switch (sm->auth_key_tx.state) {
	case AUTH_KEY_TX_NO_KEY_TRANSMIT:
		/* NOTE! IEEE 802.1aa/D4 does has this requirement as
		 * keyTxEnabled && keyAvailable && authSuccess. However, this
		 * seems to be conflicting with BE_AUTH sm, since authSuccess
		 * is now set only if keyTxEnabled is true and there are no
		 * keys to be sent.. I think the purpose is to sent the keys
		 * first and report authSuccess only after this and adding OR
		 * be_auth.aSuccess does this. */
		if (sm->keyTxEnabled && sm->keyAvailable &&
		    (sm->authSuccess /* || sm->be_auth.aSuccess */))
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	case AUTH_KEY_TX_KEY_TRANSMIT:
		if (!sm->keyTxEnabled || sm->authFail ||
		    sm->auth_pae.eapLogoff)
			SM_ENTER(AUTH_KEY_TX, NO_KEY_TRANSMIT);
		else if (sm->keyAvailable)
			SM_ENTER(AUTH_KEY_TX, KEY_TRANSMIT);
		break;
	}
}

struct eapol_state_machine *
eapol_sm_alloc(rtapd *rtapd, struct sta_info *sta)
{
	struct eapol_state_machine *sm;
	sm = (struct eapol_state_machine *) malloc(sizeof(*sm));
	if (sm == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,"IEEE 802.1X port state allocation failed\n");
		return NULL;
	}
	memset(sm, 0, sizeof(*sm));

	sm->rtapd = rtapd;
	sm->sta = sta;

	/* Set default values for state machine constants */
	sm->auth_pae.state = AUTH_PAE_INITIALIZE;
	sm->auth_pae.quietPeriod = rtapd->conf->quiet_interval;
	sm->auth_pae.initialEAPMsg = AUTH_PAE_DEFAULT_initialEAPMsg;
	sm->auth_pae.reAuthMax = AUTH_PAE_DEFAULT_reAuthMax;
	sm->auth_pae.txPeriod = AUTH_PAE_DEFAULT_txPeriod;

	sm->be_auth.state = BE_AUTH_INITIALIZE;
	sm->be_auth.suppTimeout = BE_AUTH_DEFAULT_suppTimeout;
	sm->be_auth.serverTimeout = BE_AUTH_DEFAULT_serverTimeout;
	sm->be_auth.maxReq = BE_AUTH_DEFAULT_maxReq;

	sm->reauth_timer.state = REAUTH_TIMER_INITIALIZE;

	sm->reauth_timer.reAuthEnabled = REAUTH_TIMER_DEFAULT_reAuthEnabled;
	if (rtapd->conf->session_timeout_set == 1)
	{
		sm->reauth_timer.reAuthPeriod = rtapd->conf->session_timeout_interval;
		sm->reauth_timer.reAuthEnabled = TRUE;
		DBGPRINT(RT_DEBUG_TRACE,"Set This Session Timeout Interval  %d Seconds. \n",sm->reauth_timer.reAuthPeriod );
	}
	else 
	{
		/* didn't set reauth , or  set to not reauth */
		sm->reauth_timer.reAuthPeriod = REAUTH_TIMER_DEFAULT_reAuthPeriod;
	}

	sm->portEnabled = FALSE;
	sm->portControl = Auto;
	sm->currentId = 1;
	/* IEEE 802.1aa/D4 */
	sm->keyAvailable = FALSE;
	sm->keyTxEnabled =TRUE ;
	sm->portValid = TRUE; /* TODO: should this be FALSE sometimes? */

	eapol_sm_initialize(sm);

	return sm;
}


void eapol_sm_free(struct eapol_state_machine *sm)
{
	if (sm == NULL)
		return;

	eloop_cancel_timeout(eapol_port_timers_tick, sm->rtapd, sm);

	free(sm);
}


void eapol_sm_step(struct eapol_state_machine *sm)
{
	int prev_auth_pae, prev_be_auth, prev_reauth_timer, prev_auth_key_tx;

	/* FIX: could re-run eapol_sm_step from registered timeout (after
	 * 0 sec) to make sure that other possible timeouts/events are
	 * processed */

	do {
		prev_auth_pae = sm->auth_pae.state;
		prev_be_auth = sm->be_auth.state;
		prev_reauth_timer = sm->reauth_timer.state;
		prev_auth_key_tx = sm->auth_key_tx.state;

		SM_STEP_RUN(AUTH_PAE);
		SM_STEP_RUN(BE_AUTH);
		SM_STEP_RUN(REAUTH_TIMER);
		SM_STEP_RUN(AUTH_KEY_TX);
	} while (prev_auth_pae != sm->auth_pae.state ||
		 prev_be_auth != sm->be_auth.state ||
		 prev_reauth_timer != sm->reauth_timer.state ||
		 prev_auth_key_tx != sm->auth_key_tx.state);
}


void eapol_sm_initialize(struct eapol_state_machine *sm)
{
	/* Initialize the state machines by asserting initialize and then
	 * deasserting it after one step */
	sm->initialize = TRUE;
	eapol_sm_step(sm);
	sm->initialize = FALSE;
	eapol_sm_step(sm);

	/* Start one second tick for port timers state machine */
	eloop_cancel_timeout(eapol_port_timers_tick, sm->rtapd, sm);
	eloop_register_timeout(1, 0, eapol_port_timers_tick, sm->rtapd, sm);
}

