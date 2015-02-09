#ifndef _PERF_UI_BROWSER_H_
#define _PERF_UI_BROWSER_H_ 1

#include <stdbool.h>
#include <newt.h>
#include <sys/types.h>
#include "../types.h"

#define HE_COLORSET_TOP		50
#define HE_COLORSET_MEDIUM	51
#define HE_COLORSET_NORMAL	52
#define HE_COLORSET_SELECTED	53
#define HE_COLORSET_CODE	54

struct ui_browser {
	newtComponent form, sb;
	u64	      index, top_idx;
	void	      *top, *entries;
	u16	      y, x, width, height;
	void	      *priv;
	unsigned int  (*refresh)(struct ui_browser *self);
	void	      (*write)(struct ui_browser *self, void *entry, int row);
	void	      (*seek)(struct ui_browser *self, off_t offset, int whence);
	u32	      nr_entries;
};


int ui_browser__percent_color(double percent, bool current);
bool ui_browser__is_current_entry(struct ui_browser *self, unsigned row);
void ui_browser__refresh_dimensions(struct ui_browser *self);
void ui_browser__reset_index(struct ui_browser *self);

int ui_browser__show(struct ui_browser *self, const char *title,
		     const char *helpline, ...);
void ui_browser__hide(struct ui_browser *self);
int ui_browser__refresh(struct ui_browser *self);
int ui_browser__run(struct ui_browser *self, struct newtExitStruct *es);

void ui_browser__rb_tree_seek(struct ui_browser *self, off_t offset, int whence);
unsigned int ui_browser__rb_tree_refresh(struct ui_browser *self);

void ui_browser__list_head_seek(struct ui_browser *self, off_t offset, int whence);
unsigned int ui_browser__list_head_refresh(struct ui_browser *self);

void ui_browser__init(void);
#endif /* _PERF_UI_BROWSER_H_ */
