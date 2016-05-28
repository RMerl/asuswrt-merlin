#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#define DBE 0

typedef struct {
	array *access_deny;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_craete_captcha_image_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_craete_captcha_image_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->access_deny);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_craete_captcha_image_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "url.access-deny",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->access_deny    = array_init();

		cv[0].destination = s->access_deny;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_craete_captcha_image_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(access_deny);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("url.access-deny"))) {
				PATCH(access_deny);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_craete_captcha_image_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	Cdbg(DBE, "mod_create_captcha_image %s", con->request.uri->ptr);
	if( strncmp( con->request.uri->ptr, "/GetCaptchaImage",  16 ) != 0 )
		return HANDLER_GO_ON;

	con->http_status = 200;
		
	response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));

	buffer *b = buffer_init();
	
	char* img_cap[] = {
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAH4AAQACAwAAAAAAAAAAAAAAAAADBQIEBgEBAQEBAAAAAAAAAAAAAAAAAQACBBAAAgIBAwMCAwcFAAAAAAAAAQIDBAUAERIhMRNBIlGBBvBhkcEyIxSxkiQ0FREAAAUDBAIDAQAAAAAAAAAAAAERISICEjIxQVFC8GFxkbEj/9oADAMBAAIRAxEAPwDUyuVykeUuRx3J0RJ5VVVlcAAOwAADaXMrlFr0GW5OC8DM5ErgsfPOu593XoANMrcrrlLitQgcieUF2afdiHbqeM4HX7hrYGUpVRQmmxsEq/x3KJvJsp88wAHkeQbdN+oJ107FEcDrVNPvkRwX81JibUyWbLmOaEmQSSHinCfn7t+g347/AC1HjcrlHsOr3J2AgssAZXI3WCRlP6vQjfXRQZl8l9OZCeWvEscO6JAOQTjsp2biyn19Ntc7jbldrDgUIEPgsncNPvsIJCR7pz37f00E5VRCZIdCVqvzyGKyuUkylOOS5O6PPErK0rkEF1BBBbTTFXK7ZSmq0IEJniAdWn3Ul16jlOR0+8aaWu02Ap2ZdvYZWPFnKXDJYnVzPLyVYEYA823AJsLv+GlyPF/x6HKxOAIG4EQISR55+p/yBt13+P5aZXG2Hylx1eABp5SA1mBTsXburSAj56kmw1+1Fj4a4illFdhwWeEk/vzt7f3PcNj3H5alJCcSGtUf3kWmJSiPpjJCOaVoSx5u0Sq49q/pQSsD/cNU+NjxYsPwsTk+CzuDAgG3gk5H/YPYfYd9X+MwmTg+nchSlh42Z2JiTmh5dFHcNsO3qdU9XAZWlYLW4khDwWVXnNENyYJB09/pv1+Hc9NZIylIbqpq/nDQuDZxBio8WMpTMdidnE8XFWgRQTzXYEiw234aaYrG2EylN2eAhZ4iQtmBjsHXsqyEn5aa012uwwh2Y9vYZXFZSTKXJI6c7o88rKyxOQQXYgghdZ2MfmY4qDV61lZI67KxSOQMpM852PEbjow001SQtBQWrLwxLHF9R/8ANnBS75vPBwBEvLjwn57eu2/Hf5awp087JYY2oLbqILIUypIQGeCRQByHcnYaaaHfELRz8MRYrFZSPKU5JKc6Ik8TMzROAAHUkkldNNNMrttAQs7ZD//Z",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHsAAQADAQEAAAAAAAAAAAAAAAADBAUCAQEBAQEBAAAAAAAAAAAAAAAAAgABBBAAAgIBBAECBAQHAAAAAAAAAQIDBAUAESESEzFBYYEiBlGRUhRxwdEyYiQVEQABAwMEAwEBAAAAAAAAAAABABECITESQVEiMvBhQnGR/9oADAMBAAIRAxEAPwCplcrlI8pcjjuToiTyqqrK4AAdgAAG0uZXKLXoFbk4Z4GZyJXBY+eddz9XPAA0ytyuuUuK1CByJ5QXZp92IduT1nA5+A1ZXLwURQsrQgMgrOYuZD0bzzqNu8jce/4/Ea6dBxXA9ZPNv7ukUn3I2LnkDXS/mhMbbyk9Ok/fqf079d/lqvjsrlWsuslywwEFk9Wlc7MsEjA8t6gjfWon3lk2pTWjFB5I5oYwNpOu0izE8eT/AAGrWYNRbVa6KkRlu1LMkhYyBvprs2x6Oo5DbE7b/HRe4MRXZNgQDGZ4s7+ysDFZXKSZSnHJcndHniVlaVyCC6gggtppirldspTVaECEzxAOrT7qS68jtORx8RppUytog5w7fXtMrHizlLhksTq5nl7KsCMAe7bgE2F3/LS5Hi/29DtYnAEDdCIEJI88/J/2Btzv+P8ALTK42w+UuOrwANPKQGswKdi7eqtICPnrRofbkmRagk0iCvBXPm8ciSMd55yAvjZvXf19PX3GpwAC6hEmUgI3/d17hMPjrdGeSSeVaUc0UsrzRLErCJZQUVllf9fJ/rwtZPHZXKNMs0yRx1rEcUXhXqqCCXswPmHPuPpHsOPXV/NY/MXaL0KdLwVYZYhXjDxguirL3Zvr2/u68H+OsSrgcnSsn93GkPevZCh5oQTvBIvA8noN+T7e+iCC5JroEyDHGIiWo5Y7qHFR4sZSmY7E7OJ4uqtAignuuwJFhtvy00xWNsJlKbs8BCzxEhbMDHYOvoqyEn5aaVMr6IMcOv17TK4rKSZS5JHTndHnlZWWJyCC7EEELqWalnIq9IVoLUbCuVkEaSKQfPOwDdR+Db6aaqsLK4vJsvCu44vuP/mzgpd83ng6AiXt16T99vfbfrv8tcU6Wckss1qC04FeyqmVJDsXgkUAdh7nYaaaq16racXz8KixWKykeUpySU50RJ4mZmicAAOpJJK6aaauWWllnDD67L//2Q==",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAH0AAQADAQEAAAAAAAAAAAAAAAACBAUBAwEAAwEBAAAAAAAAAAAAAAAAAAECAwQQAAICAQMCBAUDBQAAAAAAAAECAwQFABESITFBIhMGUWGBMhSRQiShUiMVFhEAAQMCBQUBAAAAAAAAAAAAAQARAiESMUFRIjJhcYGxQvD/2gAMAwEAAhEDEQA/AKmVyuUjylyOO5OiJPKqqsrgAB2AAAbXbeUyogoencsc5IGLcZX3ZvXnXc7HqdgBrmVuV1ylxWoQORPKC7NPuxDt1PGcDr8hr3/3P4Ax9qvTgWUV34EmY8N5p1IAMp6ePX499dOQ2rgesnn71Ull9yJirEkj3VcTQlXYyg+nwn5kE/t347/TVbG5XKPYdXuTsBBZYAyuRusEjKfu8CN9bNH3dkZojK8URP5MEJVQ/wBswlLbbuevkG2uZf8ACqZ+WOKpCS9WeV33kDcjBKWGySKvmA69N+vx66T4gxHhWwaMozLAgV7rHxWVykmUpxyXJ3R54lZWlcgguoIILaaYq5XbKU1WhAhM8QDq0+6kuvUcpyOnzGmnS7DJQ5s5fXVMrHizlLhksTq5nl5KsCMAebbgE2F3/TUrKYkQ48yT2CghOwECbsv5E2+/8jynfcePx+Wo5XG2Hylx1eABp5SA1mBTsXburSAj66s/8/evV6iV2haSGsx4CaNi38ib7eDN/d37eG++hww3IYkyaHvVauJu+144pJKkbVSJYlWSyOarMyy+k2wdu3m+Gql7HrVzMpv25prE1ew/qCBAhQwSqeP+f9o7Db+nXXlW9q5lqUtWWERNLPA3JnQgIizq7eVj25jWhmQtnIRRVpYZEq1LMTMZ4g3NoZF2Kl+XTbqdumopcWLuC+a0qYC6LMQ1Gz0WDio8WMpTMdidnE8XFWgRQTzXYEiw236aaYrG2EylN2eAhZ4iQtmBjsHXsqyEn6aaul2OSzY2cfrqmVxWUkylySOnO6PPKyssTkEF2IIIXU58fmY4se1etZSWOBlZo45Ayn1522PEbjoQdNNG5hgjY8uX4qwR7mlxdiOZbrOZoQqssvIpwn5+G5G/Hf6aqY3FZRLDs9OdQYLKgmJwN2gkVR9viTtppoqxa1Ms8Xv890xWKykeUpySU50RJ4mZmicAAOpJJK6aaaN12WCWyz65L//Z",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHoAAQADAQEAAAAAAAAAAAAAAAACBAUBBgEBAQADAAAAAAAAAAAAAAAAAgABAwQQAAICAgIBAgQGAwAAAAAAAAECAwQRBQAhEjETQVFhInGBkUIjFKE0BhEAAQMDBAIDAQAAAAAAAAAAAQARAiExEkFhIjLwQlFxseH/2gAMAwEAAhEDEQA/AKm12u0j2lyOO5OiJPKqqsrgAB2AAAbi5tdotegy3JwXgZnIlcFj7865P3d9ADja3K67S4rUIHInlBdmnyxDt2fGcDv6Di5crivQJoQMGgYgFp8KPfnGBicfLPfOmjCi4CS8uX78qdbYbmbXWPatWZJhPAE8ZJGfxKWCwGDnH2jP4c5R2O4FuSKxashlgsko8kmQywSMpwx9QcEc9JpVTV0q871Y4LGynijEcZkPireXiW92RyD4+XpzM2NmFf8Ao7sZqQsywzkyky+TYqucHxlC9+nQ/wA98LgmQZMxIESZVcBvuqzNVtdpJtKcclyd0eeJWVpXIILqCCC3HGquV22lNVoQITPEA6tPlSXXseU5HX1HHFTK2iDnDt7bptY9WdpcMlidXM8vkqwIwB82yATYXP6c19bp6FmKnfkeWSnSgZirxBRJiadvH7ZH7z+3B6x33jmRtdbYfaXHV4AGnlIDWYFOC7eqtICPz5fS1vaFGlXpWoIoxEzMpmqkFzNN2DIxz1j06/PPMGwYpRYSkZRLbDdaEM67G/8A23NkZuVTHG8AUIEWfxXPun7ez5N8/h31V2VeqN/clkedWaKcHECmP/VfPjIZhk+Pfp9Pry1qdlvpSv8AZtQyA2YEPjJWb+NhL7i/xn1OFx8fl8eR20uzsbWzVeeE0o45zFEZYFZWNWRQSPISfuPr+Ppw1BNuqZYxBaR56j+rC1UerG0pmOxOzieLxVoEUE+a4BIsNj9OONVrbCbSm7PAQs8RIWzAxwHX0VZCT+XHHTK+i1McOvtum11W0k2lySOnO6PPKyssTkEF2IIIXi5qto1egq05yUgZXAiclT787YP29dEHjjlyYWUcHl28KlBrNvHrZwlSwk39iu6YjcPhUsZZes9ZHFHXbg25JbFWyWaCyC7xyZLNBIqjLD1JwBxxyrWyhjx7eFR1Wq2ke0pySU50RJ4mZmicAAOpJJK8cccuWWllcMPbsv/Z",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHkAAQEBAQEAAAAAAAAAAAAAAAAEBQIDAQADAQEAAAAAAAAAAAAAAAAAAQIDBBAAAgICAgECBAUFAAAAAAAAAQIDBBEFABIhMUGBIhMGUWGRIxRx0TJCFREAAgEDAgcBAAAAAAAAAAAAAREAIQISMUHwUWFxIjJCgf/aAAwDAQACEQMRAD8Ak2u12ke0uRx3J0RJ5VVVlcAAOwAADc6sbHcPDr1gtWWklgYkJJIWdhPOM+DknC852tyuu0uK1CByJ5QXZp8sQ7eT1nA8/kOaeoswtstMq1IULwuVZTLlP3bAwvaUj2/2B9f6Y6TQArhThDNxGWpW/OSRy/cf/NnJe79b68HQky9uvSfvj3xnrn4c8qV/dJbeKzZtKf49hwkjyA5WCVlbDH2IyObl/wC6tjVN4Rxwn+LZSCPsreVcTElsOPP7Y5kx7x9lfae1UgaYVrA7j6qnqsErdcCbHn09M8QaLtEZxBtAvub37ybVbXaSbSnHJcndHniVlaVyCC6gggtxxqrldtpTVaECEzxAOrT5Ul18jtOR4/MccdMtNpLOHt9dY2serO0uGSxOrmeXsqwIwB7tkAmwuf05XXtavXz6q40s8ghhcoghQFgZrA8n6/y+Sfx/tJtdbYfaXHV4AGnlIDWYFOC7eqtICPjz2fR3rkdGGAwtIlZ/lE8TFsTzN8vVzn19fT2zwogzAZZXK2u2vObLbn7Wu15pZqbrE80azsUVSZHEpVyUfPgK368jt6TX6vYnrPL9OetZeICNXAT6Egb5/qrkgeR8v4effnlV+1Ny1OWrLCITLPA/ZnRgERZw7fIx9O45obkLYvwwVpIXjqVLMRYzxBu7QSJgqXDeMDJx49+RQFWmlXNKm132osKi3rMHVR6sbSmY7E7OJ4uqtAignuuASLDY/TjjVa2wm0puzwELPESFswMcB19FWQk/Djl0y12maOHr9dY2uq2km0uSR053R55WVlicgguxBBC87n1+5ji17161lJYoGBaOOQMjfXnbBKjIOCDxxw8kNIeDu9uDKCPuaXV2I5lus5mhCqyy9inSfv7ZIz1z8OSa3VbRLDs9OdQYLKgmJwMtBIqj/H3JxxxwqisYyna8/wB7xqtVtI9pTkkpzoiTxMzNE4AAdSSSV4444eWW2kXhh9e0/9k=",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHsAAQACAwEAAAAAAAAAAAAAAAAEBQECBgMBAAMBAQAAAAAAAAAAAAAAAAABAgQDEAACAgEEAQIGAgMAAAAAAAABAgMEBQARIRIxQSJRYYGhEwaRMiMzFBEAAQIFAwQDAAAAAAAAAAAAAQARITECEiJBYTJRcUID8JGh/9oADAMBAAIRAxEAPwCJlcrlI8pcjjuToiTyqqrK4AAdgAAG0uZXKLXoMtycF4GZyJXBY/nnXc+7ngAaZW5XXKXFahA5E8oLs0+7EO3J6zgc/IasKEFfJ2cZVajD+N67vIQ0/sjWefcL/m9T8d+T8ONaYAAkLBkaqgKp9+qi1LeetYyw1ea3PKs8ABjaR2C9J+23Uk7b9d/prWndzkVt4rdi0jCvZYJK8gO6wSsrbMfQjcauLf7O1WCzHjYYlrU5oq8QIbZgyzF29jL6x8arY89Lk7ne3VgaSOtYVXH5Qeoglbr/ALdufB9dKMcQyosLQKyT+TUXFZXKSZSnHJcndHniVlaVyCC6gggtppirldspTVaECEzxAOrT7qS68jtORx8xppwulopc2cvLdMrHizlLhksTq5nl7KsCMAe7bgE2F3/jXR/q8VTvC1eWRnFMqpeNU9rWZjvxI/PYePv6DnMrjbD5S46vAA08pAazAp2Lt5VpAR9dXOCZsdaotPJAsL1Hikb/AKIeCbEzqRtJ7vhxv9tTVxmr9cPYSaWjOPVekGV/WalOVIaplrwyxxys8SSNMzLL1fdpBv8A1Pnxv4540uYrExvFlKjPDXuV7G0SIHA3ryMx5kXr7d/b8RtxqPP+p5SKC3VrxiWN7ELwv3Ubxqs4JPYjx3G/21YX4RDVqYmKWKSSrXsfnJljQhzWkUe13Dbe7fxwOTpQeBnP6VNURlSAzNDV9FzuKjxYylMx2J2cTxdVaBFBPddgSLDbfxppisbYTKU3Z4CFniJC2YGOwdfCrISfppq4XT0XJjZx8t0yuKykmUuSR053R55WVlicgguxBBC6XMVlGr0FWnOSkDK4ETkqfzztsfbxwQdNNGTCSDY9XL4VKgT9lixU0SJdRhNCI1CyhhH0m79fXrv132+Wo2OxeVWzI8lOwO0FkdmiflmglUDcjySdNNEcmtTxeh79n7rGKxWUjylOSSnOiJPEzM0TgAB1JJJXTTTRldpJLCzy5L//2Q==",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHoAAQACAwEAAAAAAAAAAAAAAAADBQECBAYBAQEBAAAAAAAAAAAAAAAAAAACBBAAAgICAgAEBAUFAQAAAAAAAQIDBBEFABIhMYETQVFhBnGRIjIUocEjJDQVEQABAwMEAwEAAAAAAAAAAAABABECITESQWEiMvBRQrH/2gAMAwEAAhEDEQA/AOTa7XaR7S5HHcnREnlVVWVwAA7AAANyWa5vZa9Jq09uQmAtKY3kY59+dQW6n5Lj05FtblddpcVqEDkTyguzT5Yh28T1nA8foOdkf3HY1deoaVaCMTwZYH3WACT2AAO0p+ZPr+HNOgaKwA8pPMts/tRxy/cf/mzkvd9734OhJl7dek/fHxxnrn05pTt76Odv5U1tEMFkr7ryAdlgkYEdj5gjPLaj917i7ERHDB75sQQIOr9cSrKWJ/yfD2x/Xku920b7Y0PZjmWtWsMzMXGHNeRmX9DqMFcA/Hzxg8ly5BiFbRYSE5UIFdarz+q2u0k2lOOS5O6PPErK0rkEF1BBBbjjVXK7bSmq0IEJniAdWnypLr4jtOR4fUccqmVtFDnDt9bptY9WdpcMlidXM8vZVgRgD3bIBNhc/lxcj1f8eh2sTgCBuhECEke/P4n/AGBjxz8/7cbXW2H2lx1eABp5SA1mBTgu3mrSAj15m3rLL16Cq0GVgZTmzAMkzzn9OZPHz+H4efFGFUILy4/vtXP2suuqVLux7yyQ1yr9pI1jw6JIMJ1lkycSY+Hnypqvr7F+ew1md5ZYrTvmBFH6oZSx/wCg+Q8h6eHLna66Sl9swayF40nLo1oPLHHnuHfzkZR+5cD54+h5Ra3W2EsOS8BBgsjwswMctBIo8FkPz9PM8kMcpPdXIEYQxtU3uU1UerG0pmOxOzieLqrQIoJ7rgEiw2Py441WtsJtKbs8BCzxEhbMDHAdfJVkJPpxyqZX0UMcOv1um11W0k2lySOnO6PPKyssTkEF2IIIXmber2vsUPbp2O8cBDdYnyre/OwzgeBwQeOOOTCyHB5dvCtjQ3U2tsCetakmaeuV7pIzlVSxnHYZwOw/Pmmt1W0Sw7PTnUGCyoJicDLQSKo/b8SccccVY2Ti8e3hTVaraR7SnJJTnREniZmaJwAA6kkkrxxxxyy0snDD67L/2Q==",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHsAAQACAwAAAAAAAAAAAAAAAAAEBQEDBgEAAwEBAAAAAAAAAAAAAAAAAAECAwQQAAICAQQABAUEAwAAAAAAAAECAwQFABEhEjGBExRRYSJCBkGRMiPwUhYRAAEDAwMEAwAAAAAAAAAAAAEAEQIhMRJBIjJRYXFC8IGh/9oADAMBAAIRAxEAPwCJlcrlI8pcjjuToiTyqqrK4AAdgAAG0uZXKLXoMtycF4GZyJXBY+vOu5+rngAaZW5XXKXFahA5E8oLs0+7EO3J6zgc/Iat8GtK7dx0c1OFQKryx7GU9WWxMAoDysCPu5B10lgAWXCATIgSv56qBBL+RyYqeVXuuxmhMbgyk+n0n79T/rv138taMdlcq1l1kuWGAgsnq0rnZlgkYHlvEEb6urH5Zk41tsIIg9eykCIwf+LCf+Q7jn6B/m2tmc9pDk45PaRGxYqWZJuxcMOsEnB9ORV52Kk7b/PUvcGIr0VYihjM7SHfyqDFZXKSZSnHJcndHniVlaVyCC6gggtppirldspTVaECEzxAOrT7qS68jtORx8xpqqZW0UOcOXt3TKx4s5S4ZLE6uZ5eyrAjAHu24BNhd/21YY+ilmzjBj7U8c8cDOkvoJsqCefdn/v45O23O/ntqvyuNsPlLjq8ADTykBrMCnYu3irSAjz10P45GK/owTSxI81Qxxus0Tt39xO2yBHJbhvEccbeOlItEMVUIvMgxYPevVTmyOAWd2naJrMUkSTWfT2j9x1l9NjyfDZud+Pj8KfJUPb5ZpshblmlsVrJR0gUR9BBIGC/3/aD4bc/HnfUb/lc0lKzWEIdmsQsjB06siLOrNywP3jx51a5OErDSx4liknq1bCzFpY1YE1nQfS7Btj8f0HJ1NAaF1ocpDdHFiCL3f8AVzmKjxYylMx2J2cTxdVaBFBPddgSLDbftppisbYTKU3Z4CFniJC2YGOwdfBVkJPlpq6ZX0WLHDj7d0yuKykmUuSR053R55WVlicgguxBBC6zcxWVNegEp2C0cDBgIn3VvXnbY/TwdiDppo3MLIODy5fCpUf/AE4xc6EXRIJoRGNpe/p9Zu+369d+u/lqLjsXlhakeWnYHaCyOzRPyzQSqBuR4knbTTRWrYp0eL5/flYxWKykeUpySU50RJ4mZmicAAOpJJK6aaaN2WlktmHtyX//2Q==",
		"data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAZABkAAD/7AARRHVja3kAAQAEAAAAHgAA/+4ADkFkb2JlAGTAAAAAAf/bAIQAEAsLCwwLEAwMEBcPDQ8XGxQQEBQbHxcXFxcXHx4XGhoaGhceHiMlJyUjHi8vMzMvL0BAQEBAQEBAQEBAQEBAQAERDw8RExEVEhIVFBEUERQaFBYWFBomGhocGhomMCMeHh4eIzArLicnJy4rNTUwMDU1QEA/QEBAQEBAQEBAQEBA/8AAEQgAKAAoAwEiAAIRAQMRAf/EAHcAAQEBAQEAAAAAAAAAAAAAAAAFBAMCAQADAQEAAAAAAAAAAAAAAAAAAQIDBBAAAgIBAwMDBAMBAAAAAAAAAQIDBAUAERIhMSITFAZBUWGBkTJCUhEAAQMDAwQDAAAAAAAAAAAAAQARAiExEkEiMlFhcULwgbH/2gAMAwEAAhEDEQA/AMmVyuUjylyOO5OiJPKqqsrgAB2AAAbS5lcotegy3JwXgZnIlcFj6867ny69ABplbldcpcVqEDkTyguzT7sQ7dTxnA6/gauYOGjb9vZnpxBKlUyqFMhKt7ifYAPIwI8SeoPX666CQACy4QDKUgJX89VLim+Rtip5ed0t60JR95SfT4T8+J/5347/AK1nxuVyj2XV7k7AQWW2aVz1WCRlPVvoRvqsvzPItUntCGEGOaKNV2f+kizMd/Pv4DWjKJTSxXyMNSIrkKlmVmYyBtxXd9iEkVfIHY9N+/XfSe4MRVUwLGMycWd/Kg4rK5STKU45Lk7o88SsrSuQQXUEEFtNMVcrtlKarQgQmeIB1afdSXXqOU5HT8jTVUytooc4cvbumVjxZylwyWJ1czy8lWBGAPNtwCbC7/xqvisni8Z7RZZZXgsVjGeUSqNvcT+T8ZWI7sNgD9/xqRlcbYfKXHV4AGnlIDWYFOxdu6tICP3rrJhL9uKjDX9KSRKzeCzxEnaeZvHZzv379tIsQASmMhKREa/fVVW+OYlKc+2UjFOSaKXn4txWNZQE5c+pIf7fTtrblWx7U8eqNJHAa1k19kDH0hVYdeTp/k7j7/jvqRR+KZRqsta2grI88MjOXRto41nDkcGbr5jWnJt7zJLHUeA0qtSxDDtYh3JavIu/H1NwOw6/bc9NTc8nZ/xaCkeGLsGr1UbFR4sZSmY7E7OJ4uKtAignmuwJFhtv400xWNsJlKbs8BCzxEhbMDHYOvZVkJP601dMr6LJjhx9u6ZXFZSTKXJI6c7o88rKyxOQQXYgghde56Gaiix7161lJYoGUtHHIGQmedtiVG46EHTTRVhZG15cvhXaSP5HZxU8dmO5I5mh4o6SElOE/Ppt2347/rWbG4rKJYdnpzqDBZUExOBu0EiqP6/UnbTTRVi2KNrxfN+/lMVispHlKcklOdESeJmZonAADqSSSummmjdlpZGzD25L/9k="
	};
	
	int captcha_num[4];
	
	captcha_num[0] = (rand() % 9);
	captcha_num[1] = (rand() % 9);
	captcha_num[2] = (rand() % 9);
	captcha_num[3] = (rand() % 9);
	
	char captcha_code[10];
	sprintf(captcha_code,"%d%d%d%d", captcha_num[0]+1, captcha_num[1]+1, captcha_num[2]+1, captcha_num[3]+1);
	
	buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
	buffer_append_string_len(b,CONST_STR_LEN("<result>"));
	buffer_append_string_len(b,CONST_STR_LEN("<enable>"));

	#if EMBEDDED_EANBLE
	#ifndef APP_IPKG
	buffer_append_string(b,nvram_get_enable_webdav_captcha());
	#else
	char *enable_webdav_captcha = nvram_get_enable_webdav_captcha();
   	buffer_append_string(b,enable_webdav_captcha);
   	free(enable_webdav_captcha);
	#endif
	#else
	buffer_append_string(b,"0");
	#endif
	
	buffer_append_string_len(b,CONST_STR_LEN("</enable>"));
	buffer_append_string_len(b,CONST_STR_LEN("<img1>"));
	buffer_append_string(b,img_cap[captcha_num[0]]);
	buffer_append_string_len(b,CONST_STR_LEN("</img1>"));
	buffer_append_string_len(b,CONST_STR_LEN("<img2>"));
	buffer_append_string(b,img_cap[captcha_num[1]]);
	buffer_append_string_len(b,CONST_STR_LEN("</img2>"));
	buffer_append_string_len(b,CONST_STR_LEN("<img3>"));
	buffer_append_string(b,img_cap[captcha_num[2]]);
	buffer_append_string_len(b,CONST_STR_LEN("</img3>"));
	buffer_append_string_len(b,CONST_STR_LEN("<img4>"));
	buffer_append_string(b,img_cap[captcha_num[3]]);
	buffer_append_string_len(b,CONST_STR_LEN("</img4>"));
	buffer_append_string_len(b,CONST_STR_LEN("<code>"));
	buffer_append_string(b,captcha_code);
	buffer_append_string_len(b,CONST_STR_LEN("</code>"));
	buffer_append_string_len(b,CONST_STR_LEN("</result>"));

	chunkqueue_append_buffer(con->write_queue, b);
	buffer_free(b);
					
	con->file_finished = 1;
	return HANDLER_FINISHED;
		
	/* not found */
	//return HANDLER_GO_ON;
}
#ifndef APP_IPKG
int mod_create_captcha_image_plugin_init(plugin *p);
int mod_create_captcha_image_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("create_captcha_image");

	p->init        = mod_craete_captcha_image_init;
	p->set_defaults = mod_craete_captcha_image_set_defaults;
	p->handle_physical = mod_craete_captcha_image_physical_handler;
	p->cleanup     = mod_craete_captcha_image_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_create_captcha_image_plugin_init(plugin *p);
int aicloud_mod_create_captcha_image_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("create_captcha_image");

	p->init        = mod_craete_captcha_image_init;
	p->set_defaults = mod_craete_captcha_image_set_defaults;
	p->handle_physical = mod_craete_captcha_image_physical_handler;
	p->cleanup     = mod_craete_captcha_image_free;

	p->data        = NULL;

	return 0;
}
#endif