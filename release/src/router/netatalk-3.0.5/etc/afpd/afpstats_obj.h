#ifndef AFPSTATS_OBJ_H
#define AFPSTATS_OBJ_H

#include <glib.h>

typedef struct AFPStatsObj AFPStatsObj;
typedef struct AFPStatsObjClass AFPStatsObjClass;

GType    afpstats_obj_get_type(void);
gboolean afpstats_obj_get_users(AFPStatsObj *obj, gchar ***ret, GError **error);

#define AFPSTATS_TYPE_OBJECT              (afpstats_obj_get_type ())
#define AFPSTATS_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), AFPSTATS_TYPE_OBJECT, AFPStatsObj))
#define AFPSTATS_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), AFPSTATS_TYPE_OBJECT, AFPStatsObjClass))
#define AFPSTATS_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), AFPSTATS_TYPE_OBJECT))
#define AFPSTATS_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), AFPSTATS_TYPE_OBJECT))
#define AFPSTATS_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), AFPSTATS_TYPE_OBJECT, AFPStatsObjClass))

#endif /* AFPSTATS_OBJ_H */
