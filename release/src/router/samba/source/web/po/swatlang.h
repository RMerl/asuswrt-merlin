#ifndef  __SWATLANG_H_DEFINED__
#define  __SWATLANG_H_DEFINED__

typedef struct {
	char *name;
	char *lang;
}	SwatLang;

static SwatLang swatlang[] = {
	"English", "",
	"Japanese", "ja_JP.ujis",
	NULL, NULL };
#endif
