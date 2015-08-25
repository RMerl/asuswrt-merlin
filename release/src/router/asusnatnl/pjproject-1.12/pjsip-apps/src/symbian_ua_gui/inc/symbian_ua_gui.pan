
#ifndef SYMBIAN_UA_GUI_PAN_H
#define SYMBIAN_UA_GUI_PAN_H

/** symbian_ua_gui application panic codes */
enum Tsymbian_ua_guiPanics
	{
	Esymbian_ua_guiUi = 1
	// add further panics here
	};

inline void Panic(Tsymbian_ua_guiPanics aReason)
	{
	_LIT(applicationName,"symbian_ua_gui");
	User::Panic(applicationName, aReason);
	}

#endif // SYMBIAN_UA_GUI_PAN_H
