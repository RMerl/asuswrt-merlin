#include "GeoIP.h"
#include <string.h>
const char * GeoIP_time_zone_by_country_and_region(const char * country,
                                                   const char * region)
{
    const char * timezone = NULL;
    if (country == NULL) {
        return NULL;
    }
    if (region == NULL) {
        region = "";
    }
    if (strcmp(country, "AD") == 0) {
        timezone = "Europe/Andorra";
    }else if (strcmp(country, "AE") == 0) {
        timezone = "Asia/Dubai";
    }else if (strcmp(country, "AF") == 0) {
        timezone = "Asia/Kabul";
    }else if (strcmp(country, "AG") == 0) {
        timezone = "America/Antigua";
    }else if (strcmp(country, "AI") == 0) {
        timezone = "America/Anguilla";
    }else if (strcmp(country, "AL") == 0) {
        timezone = "Europe/Tirane";
    }else if (strcmp(country, "AM") == 0) {
        timezone = "Asia/Yerevan";
    }else if (strcmp(country, "AN") == 0) {
        timezone = "America/Curacao";
    }else if (strcmp(country, "AO") == 0) {
        timezone = "Africa/Luanda";
    }else if (strcmp(country, "AQ") == 0) {
        timezone = "Antarctica/South_Pole";
    }else if (strcmp(country, "AR") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "02") == 0) {
            timezone = "America/Argentina/Catamarca";
        }else if (strcmp(region, "03") == 0) {
            timezone = "America/Argentina/Tucuman";
        }else if (strcmp(region, "04") == 0) {
            timezone = "America/Argentina/Rio_Gallegos";
        }else if (strcmp(region, "05") == 0) {
            timezone = "America/Argentina/Cordoba";
        }else if (strcmp(region, "06") == 0) {
            timezone = "America/Argentina/Tucuman";
        }else if (strcmp(region, "07") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "08") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "09") == 0) {
            timezone = "America/Argentina/Tucuman";
        }else if (strcmp(region, "10") == 0) {
            timezone = "America/Argentina/Jujuy";
        }else if (strcmp(region, "11") == 0) {
            timezone = "America/Argentina/San_Luis";
        }else if (strcmp(region, "12") == 0) {
            timezone = "America/Argentina/La_Rioja";
        }else if (strcmp(region, "13") == 0) {
            timezone = "America/Argentina/Mendoza";
        }else if (strcmp(region, "14") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "15") == 0) {
            timezone = "America/Argentina/San_Luis";
        }else if (strcmp(region, "16") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "17") == 0) {
            timezone = "America/Argentina/Salta";
        }else if (strcmp(region, "18") == 0) {
            timezone = "America/Argentina/San_Juan";
        }else if (strcmp(region, "19") == 0) {
            timezone = "America/Argentina/San_Luis";
        }else if (strcmp(region, "20") == 0) {
            timezone = "America/Argentina/Rio_Gallegos";
        }else if (strcmp(region, "21") == 0) {
            timezone = "America/Argentina/Buenos_Aires";
        }else if (strcmp(region, "22") == 0) {
            timezone = "America/Argentina/Catamarca";
        }else if (strcmp(region, "23") == 0) {
            timezone = "America/Argentina/Ushuaia";
        }else if (strcmp(region, "24") == 0) {
            timezone = "America/Argentina/Tucuman";
        }
    }else if (strcmp(country, "AS") == 0) {
        timezone = "Pacific/Pago_Pago";
    }else if (strcmp(country, "AT") == 0) {
        timezone = "Europe/Vienna";
    }else if (strcmp(country, "AU") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Australia/Sydney";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Australia/Sydney";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Australia/Darwin";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Australia/Brisbane";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Australia/Adelaide";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Australia/Hobart";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Australia/Melbourne";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Australia/Perth";
        }
    }else if (strcmp(country, "AW") == 0) {
        timezone = "America/Aruba";
    }else if (strcmp(country, "AX") == 0) {
        timezone = "Europe/Mariehamn";
    }else if (strcmp(country, "AZ") == 0) {
        timezone = "Asia/Baku";
    }else if (strcmp(country, "BA") == 0) {
        timezone = "Europe/Sarajevo";
    }else if (strcmp(country, "BB") == 0) {
        timezone = "America/Barbados";
    }else if (strcmp(country, "BD") == 0) {
        timezone = "Asia/Dhaka";
    }else if (strcmp(country, "BE") == 0) {
        timezone = "Europe/Brussels";
    }else if (strcmp(country, "BF") == 0) {
        timezone = "Africa/Ouagadougou";
    }else if (strcmp(country, "BG") == 0) {
        timezone = "Europe/Sofia";
    }else if (strcmp(country, "BH") == 0) {
        timezone = "Asia/Bahrain";
    }else if (strcmp(country, "BI") == 0) {
        timezone = "Africa/Bujumbura";
    }else if (strcmp(country, "BJ") == 0) {
        timezone = "Africa/Porto-Novo";
    }else if (strcmp(country, "BL") == 0) {
        timezone = "America/St_Barthelemy";
    }else if (strcmp(country, "BM") == 0) {
        timezone = "Atlantic/Bermuda";
    }else if (strcmp(country, "BN") == 0) {
        timezone = "Asia/Brunei";
    }else if (strcmp(country, "BO") == 0) {
        timezone = "America/La_Paz";
    }else if (strcmp(country, "BQ") == 0) {
        timezone = "America/Curacao";
    }else if (strcmp(country, "BR") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "America/Rio_Branco";
        }else if (strcmp(region, "02") == 0) {
            timezone = "America/Maceio";
        }else if (strcmp(region, "03") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "04") == 0) {
            timezone = "America/Manaus";
        }else if (strcmp(region, "05") == 0) {
            timezone = "America/Bahia";
        }else if (strcmp(region, "06") == 0) {
            timezone = "America/Fortaleza";
        }else if (strcmp(region, "07") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "08") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "11") == 0) {
            timezone = "America/Campo_Grande";
        }else if (strcmp(region, "13") == 0) {
            timezone = "America/Belem";
        }else if (strcmp(region, "14") == 0) {
            timezone = "America/Cuiaba";
        }else if (strcmp(region, "15") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "16") == 0) {
            timezone = "America/Belem";
        }else if (strcmp(region, "17") == 0) {
            timezone = "America/Recife";
        }else if (strcmp(region, "18") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "20") == 0) {
            timezone = "America/Fortaleza";
        }else if (strcmp(region, "21") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "22") == 0) {
            timezone = "America/Recife";
        }else if (strcmp(region, "23") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "24") == 0) {
            timezone = "America/Porto_Velho";
        }else if (strcmp(region, "25") == 0) {
            timezone = "America/Boa_Vista";
        }else if (strcmp(region, "26") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "27") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "28") == 0) {
            timezone = "America/Maceio";
        }else if (strcmp(region, "29") == 0) {
            timezone = "America/Sao_Paulo";
        }else if (strcmp(region, "30") == 0) {
            timezone = "America/Recife";
        }else if (strcmp(region, "31") == 0) {
            timezone = "America/Araguaina";
        }
    }else if (strcmp(country, "BS") == 0) {
        timezone = "America/Nassau";
    }else if (strcmp(country, "BT") == 0) {
        timezone = "Asia/Thimphu";
    }else if (strcmp(country, "BV") == 0) {
        timezone = "Antarctica/Syowa";
    }else if (strcmp(country, "BW") == 0) {
        timezone = "Africa/Gaborone";
    }else if (strcmp(country, "BY") == 0) {
        timezone = "Europe/Minsk";
    }else if (strcmp(country, "BZ") == 0) {
        timezone = "America/Belize";
    }else if (strcmp(country, "CA") == 0) {
        if (strcmp(region, "AB") == 0) {
            timezone = "America/Edmonton";
        }else if (strcmp(region, "BC") == 0) {
            timezone = "America/Vancouver";
        }else if (strcmp(region, "MB") == 0) {
            timezone = "America/Winnipeg";
        }else if (strcmp(region, "NB") == 0) {
            timezone = "America/Halifax";
        }else if (strcmp(region, "NL") == 0) {
            timezone = "America/St_Johns";
        }else if (strcmp(region, "NS") == 0) {
            timezone = "America/Halifax";
        }else if (strcmp(region, "NT") == 0) {
            timezone = "America/Yellowknife";
        }else if (strcmp(region, "NU") == 0) {
            timezone = "America/Rankin_Inlet";
        }else if (strcmp(region, "ON") == 0) {
            timezone = "America/Toronto";
        }else if (strcmp(region, "PE") == 0) {
            timezone = "America/Halifax";
        }else if (strcmp(region, "QC") == 0) {
            timezone = "America/Toronto";
        }else if (strcmp(region, "SK") == 0) {
            timezone = "America/Regina";
        }else if (strcmp(region, "YT") == 0) {
            timezone = "America/Whitehorse";
        }
    }else if (strcmp(country, "CC") == 0) {
        timezone = "Indian/Cocos";
    }else if (strcmp(country, "CD") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Africa/Kinshasa";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Africa/Kinshasa";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Africa/Kinshasa";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Africa/Kinshasa";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Africa/Kinshasa";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Africa/Lubumbashi";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Africa/Lubumbashi";
        }
    }else if (strcmp(country, "CF") == 0) {
        timezone = "Africa/Bangui";
    }else if (strcmp(country, "CG") == 0) {
        timezone = "Africa/Brazzaville";
    }else if (strcmp(country, "CH") == 0) {
        timezone = "Europe/Zurich";
    }else if (strcmp(country, "CI") == 0) {
        timezone = "Africa/Abidjan";
    }else if (strcmp(country, "CK") == 0) {
        timezone = "Pacific/Rarotonga";
    }else if (strcmp(country, "CL") == 0) {
        timezone = "America/Santiago";
    }else if (strcmp(country, "CM") == 0) {
        timezone = "Africa/Lagos";
    }else if (strcmp(country, "CN") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Harbin";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Asia/Harbin";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Urumqi";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "18") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "19") == 0) {
            timezone = "Asia/Harbin";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Asia/Harbin";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "22") == 0) {
            timezone = "Asia/Harbin";
        }else if (strcmp(region, "23") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "24") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "25") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "26") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "28") == 0) {
            timezone = "Asia/Shanghai";
        }else if (strcmp(region, "29") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "30") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "31") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "32") == 0) {
            timezone = "Asia/Chongqing";
        }else if (strcmp(region, "33") == 0) {
            timezone = "Asia/Chongqing";
        }
    }else if (strcmp(country, "CO") == 0) {
        timezone = "America/Bogota";
    }else if (strcmp(country, "CR") == 0) {
        timezone = "America/Costa_Rica";
    }else if (strcmp(country, "CU") == 0) {
        timezone = "America/Havana";
    }else if (strcmp(country, "CV") == 0) {
        timezone = "Atlantic/Cape_Verde";
    }else if (strcmp(country, "CW") == 0) {
        timezone = "America/Curacao";
    }else if (strcmp(country, "CX") == 0) {
        timezone = "Indian/Christmas";
    }else if (strcmp(country, "CY") == 0) {
        timezone = "Asia/Nicosia";
    }else if (strcmp(country, "CZ") == 0) {
        timezone = "Europe/Prague";
    }else if (strcmp(country, "DE") == 0) {
        timezone = "Europe/Berlin";
    }else if (strcmp(country, "DJ") == 0) {
        timezone = "Africa/Djibouti";
    }else if (strcmp(country, "DK") == 0) {
        timezone = "Europe/Copenhagen";
    }else if (strcmp(country, "DM") == 0) {
        timezone = "America/Dominica";
    }else if (strcmp(country, "DO") == 0) {
        timezone = "America/Santo_Domingo";
    }else if (strcmp(country, "DZ") == 0) {
        timezone = "Africa/Algiers";
    }else if (strcmp(country, "EC") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Pacific/Galapagos";
        }else if (strcmp(region, "02") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "03") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "04") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "05") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "06") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "07") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "08") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "09") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "10") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "11") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "12") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "13") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "14") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "15") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "17") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "18") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "19") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "20") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "22") == 0) {
            timezone = "America/Guayaquil";
        }else if (strcmp(region, "24") == 0) {
            timezone = "America/Guayaquil";
        }
    }else if (strcmp(country, "EE") == 0) {
        timezone = "Europe/Tallinn";
    }else if (strcmp(country, "EG") == 0) {
        timezone = "Africa/Cairo";
    }else if (strcmp(country, "EH") == 0) {
        timezone = "Africa/El_Aaiun";
    }else if (strcmp(country, "ER") == 0) {
        timezone = "Africa/Asmara";
    }else if (strcmp(country, "ES") == 0) {
        if (strcmp(region, "07") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "27") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "29") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "31") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "32") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "34") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "39") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "51") == 0) {
            timezone = "Africa/Ceuta";
        }else if (strcmp(region, "52") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "53") == 0) {
            timezone = "Atlantic/Canary";
        }else if (strcmp(region, "54") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "55") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "56") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "57") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "58") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "59") == 0) {
            timezone = "Europe/Madrid";
        }else if (strcmp(region, "60") == 0) {
            timezone = "Europe/Madrid";
        }
    }else if (strcmp(country, "ET") == 0) {
        timezone = "Africa/Addis_Ababa";
    }else if (strcmp(country, "FI") == 0) {
        timezone = "Europe/Helsinki";
    }else if (strcmp(country, "FJ") == 0) {
        timezone = "Pacific/Fiji";
    }else if (strcmp(country, "FK") == 0) {
        timezone = "Atlantic/Stanley";
    }else if (strcmp(country, "FM") == 0) {
        timezone = "Pacific/Pohnpei";
    }else if (strcmp(country, "FO") == 0) {
        timezone = "Atlantic/Faroe";
    }else if (strcmp(country, "FR") == 0) {
        timezone = "Europe/Paris";
    }else if (strcmp(country, "FX") == 0) {
        timezone = "Europe/Paris";
    }else if (strcmp(country, "GA") == 0) {
        timezone = "Africa/Libreville";
    }else if (strcmp(country, "GB") == 0) {
        timezone = "Europe/London";
    }else if (strcmp(country, "GD") == 0) {
        timezone = "America/Grenada";
    }else if (strcmp(country, "GE") == 0) {
        timezone = "Asia/Tbilisi";
    }else if (strcmp(country, "GF") == 0) {
        timezone = "America/Cayenne";
    }else if (strcmp(country, "GG") == 0) {
        timezone = "Europe/Guernsey";
    }else if (strcmp(country, "GH") == 0) {
        timezone = "Africa/Accra";
    }else if (strcmp(country, "GI") == 0) {
        timezone = "Europe/Gibraltar";
    }else if (strcmp(country, "GL") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "America/Thule";
        }else if (strcmp(region, "02") == 0) {
            timezone = "America/Godthab";
        }else if (strcmp(region, "03") == 0) {
            timezone = "America/Godthab";
        }
    }else if (strcmp(country, "GM") == 0) {
        timezone = "Africa/Banjul";
    }else if (strcmp(country, "GN") == 0) {
        timezone = "Africa/Conakry";
    }else if (strcmp(country, "GP") == 0) {
        timezone = "America/Guadeloupe";
    }else if (strcmp(country, "GQ") == 0) {
        timezone = "Africa/Malabo";
    }else if (strcmp(country, "GR") == 0) {
        timezone = "Europe/Athens";
    }else if (strcmp(country, "GS") == 0) {
        timezone = "Atlantic/South_Georgia";
    }else if (strcmp(country, "GT") == 0) {
        timezone = "America/Guatemala";
    }else if (strcmp(country, "GU") == 0) {
        timezone = "Pacific/Guam";
    }else if (strcmp(country, "GW") == 0) {
        timezone = "Africa/Bissau";
    }else if (strcmp(country, "GY") == 0) {
        timezone = "America/Guyana";
    }else if (strcmp(country, "HK") == 0) {
        timezone = "Asia/Hong_Kong";
    }else if (strcmp(country, "HN") == 0) {
        timezone = "America/Tegucigalpa";
    }else if (strcmp(country, "HR") == 0) {
        timezone = "Europe/Zagreb";
    }else if (strcmp(country, "HT") == 0) {
        timezone = "America/Port-au-Prince";
    }else if (strcmp(country, "HU") == 0) {
        timezone = "Europe/Budapest";
    }else if (strcmp(country, "ID") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Asia/Jayapura";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "18") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "19") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "22") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "23") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "24") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "25") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "26") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "28") == 0) {
            timezone = "Asia/Jayapura";
        }else if (strcmp(region, "29") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "30") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "31") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "32") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "33") == 0) {
            timezone = "Asia/Jakarta";
        }else if (strcmp(region, "34") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "35") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "36") == 0) {
            timezone = "Asia/Jayapura";
        }else if (strcmp(region, "37") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "38") == 0) {
            timezone = "Asia/Makassar";
        }else if (strcmp(region, "39") == 0) {
            timezone = "Asia/Jayapura";
        }else if (strcmp(region, "40") == 0) {
            timezone = "Asia/Pontianak";
        }else if (strcmp(region, "41") == 0) {
            timezone = "Asia/Makassar";
        }
    }else if (strcmp(country, "IE") == 0) {
        timezone = "Europe/Dublin";
    }else if (strcmp(country, "IL") == 0) {
        timezone = "Asia/Jerusalem";
    }else if (strcmp(country, "IM") == 0) {
        timezone = "Europe/Isle_of_Man";
    }else if (strcmp(country, "IN") == 0) {
        timezone = "Asia/Kolkata";
    }else if (strcmp(country, "IO") == 0) {
        timezone = "Indian/Chagos";
    }else if (strcmp(country, "IQ") == 0) {
        timezone = "Asia/Baghdad";
    }else if (strcmp(country, "IR") == 0) {
        timezone = "Asia/Tehran";
    }else if (strcmp(country, "IS") == 0) {
        timezone = "Atlantic/Reykjavik";
    }else if (strcmp(country, "IT") == 0) {
        timezone = "Europe/Rome";
    }else if (strcmp(country, "JE") == 0) {
        timezone = "Europe/Jersey";
    }else if (strcmp(country, "JM") == 0) {
        timezone = "America/Jamaica";
    }else if (strcmp(country, "JO") == 0) {
        timezone = "Asia/Amman";
    }else if (strcmp(country, "JP") == 0) {
        timezone = "Asia/Tokyo";
    }else if (strcmp(country, "KE") == 0) {
        timezone = "Africa/Nairobi";
    }else if (strcmp(country, "KG") == 0) {
        timezone = "Asia/Bishkek";
    }else if (strcmp(country, "KH") == 0) {
        timezone = "Asia/Phnom_Penh";
    }else if (strcmp(country, "KI") == 0) {
        timezone = "Pacific/Tarawa";
    }else if (strcmp(country, "KM") == 0) {
        timezone = "Indian/Comoro";
    }else if (strcmp(country, "KN") == 0) {
        timezone = "America/St_Kitts";
    }else if (strcmp(country, "KP") == 0) {
        timezone = "Asia/Pyongyang";
    }else if (strcmp(country, "KR") == 0) {
        timezone = "Asia/Seoul";
    }else if (strcmp(country, "KW") == 0) {
        timezone = "Asia/Kuwait";
    }else if (strcmp(country, "KY") == 0) {
        timezone = "America/Cayman";
    }else if (strcmp(country, "KZ") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Asia/Almaty";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Almaty";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Asia/Aqtobe";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Asia/Aqtau";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Asia/Oral";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Asia/Aqtau";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Almaty";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Aqtobe";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Qyzylorda";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Asia/Almaty";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Asia/Aqtobe";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Asia/Almaty";
        }
    }else if (strcmp(country, "LA") == 0) {
        timezone = "Asia/Vientiane";
    }else if (strcmp(country, "LB") == 0) {
        timezone = "Asia/Beirut";
    }else if (strcmp(country, "LC") == 0) {
        timezone = "America/St_Lucia";
    }else if (strcmp(country, "LI") == 0) {
        timezone = "Europe/Vaduz";
    }else if (strcmp(country, "LK") == 0) {
        timezone = "Asia/Colombo";
    }else if (strcmp(country, "LR") == 0) {
        timezone = "Africa/Monrovia";
    }else if (strcmp(country, "LS") == 0) {
        timezone = "Africa/Maseru";
    }else if (strcmp(country, "LT") == 0) {
        timezone = "Europe/Vilnius";
    }else if (strcmp(country, "LU") == 0) {
        timezone = "Europe/Luxembourg";
    }else if (strcmp(country, "LV") == 0) {
        timezone = "Europe/Riga";
    }else if (strcmp(country, "LY") == 0) {
        timezone = "Africa/Tripoli";
    }else if (strcmp(country, "MA") == 0) {
        timezone = "Africa/Casablanca";
    }else if (strcmp(country, "MC") == 0) {
        timezone = "Europe/Monaco";
    }else if (strcmp(country, "MD") == 0) {
        timezone = "Europe/Chisinau";
    }else if (strcmp(country, "ME") == 0) {
        timezone = "Europe/Podgorica";
    }else if (strcmp(country, "MF") == 0) {
        timezone = "America/Marigot";
    }else if (strcmp(country, "MG") == 0) {
        timezone = "Indian/Antananarivo";
    }else if (strcmp(country, "MH") == 0) {
        timezone = "Pacific/Kwajalein";
    }else if (strcmp(country, "MK") == 0) {
        timezone = "Europe/Skopje";
    }else if (strcmp(country, "ML") == 0) {
        timezone = "Africa/Bamako";
    }else if (strcmp(country, "MM") == 0) {
        timezone = "Asia/Rangoon";
    }else if (strcmp(country, "MN") == 0) {
        if (strcmp(region, "06") == 0) {
            timezone = "Asia/Choibalsan";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Ulaanbaatar";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Asia/Choibalsan";
        }else if (strcmp(region, "19") == 0) {
            timezone = "Asia/Hovd";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Asia/Ulaanbaatar";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Asia/Ulaanbaatar";
        }else if (strcmp(region, "25") == 0) {
            timezone = "Asia/Ulaanbaatar";
        }
    }else if (strcmp(country, "MO") == 0) {
        timezone = "Asia/Macau";
    }else if (strcmp(country, "MP") == 0) {
        timezone = "Pacific/Saipan";
    }else if (strcmp(country, "MQ") == 0) {
        timezone = "America/Martinique";
    }else if (strcmp(country, "MR") == 0) {
        timezone = "Africa/Nouakchott";
    }else if (strcmp(country, "MS") == 0) {
        timezone = "America/Montserrat";
    }else if (strcmp(country, "MT") == 0) {
        timezone = "Europe/Malta";
    }else if (strcmp(country, "MU") == 0) {
        timezone = "Indian/Mauritius";
    }else if (strcmp(country, "MV") == 0) {
        timezone = "Indian/Maldives";
    }else if (strcmp(country, "MW") == 0) {
        timezone = "Africa/Blantyre";
    }else if (strcmp(country, "MX") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "02") == 0) {
            timezone = "America/Tijuana";
        }else if (strcmp(region, "03") == 0) {
            timezone = "America/Hermosillo";
        }else if (strcmp(region, "04") == 0) {
            timezone = "America/Merida";
        }else if (strcmp(region, "05") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "06") == 0) {
            timezone = "America/Chihuahua";
        }else if (strcmp(region, "07") == 0) {
            timezone = "America/Monterrey";
        }else if (strcmp(region, "08") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "09") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "10") == 0) {
            timezone = "America/Mazatlan";
        }else if (strcmp(region, "11") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "12") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "13") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "14") == 0) {
            timezone = "America/Mazatlan";
        }else if (strcmp(region, "15") == 0) {
            timezone = "America/Chihuahua";
        }else if (strcmp(region, "16") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "17") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "18") == 0) {
            timezone = "America/Mazatlan";
        }else if (strcmp(region, "19") == 0) {
            timezone = "America/Monterrey";
        }else if (strcmp(region, "20") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "21") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "22") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "23") == 0) {
            timezone = "America/Cancun";
        }else if (strcmp(region, "24") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "25") == 0) {
            timezone = "America/Mazatlan";
        }else if (strcmp(region, "26") == 0) {
            timezone = "America/Hermosillo";
        }else if (strcmp(region, "27") == 0) {
            timezone = "America/Merida";
        }else if (strcmp(region, "28") == 0) {
            timezone = "America/Monterrey";
        }else if (strcmp(region, "29") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "30") == 0) {
            timezone = "America/Mexico_City";
        }else if (strcmp(region, "31") == 0) {
            timezone = "America/Merida";
        }else if (strcmp(region, "32") == 0) {
            timezone = "America/Monterrey";
        }
    }else if (strcmp(country, "MY") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Kuching";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Kuala_Lumpur";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Asia/Kuching";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Asia/Kuching";
        }
    }else if (strcmp(country, "MZ") == 0) {
        timezone = "Africa/Maputo";
    }else if (strcmp(country, "NA") == 0) {
        timezone = "Africa/Windhoek";
    }else if (strcmp(country, "NC") == 0) {
        timezone = "Pacific/Noumea";
    }else if (strcmp(country, "NE") == 0) {
        timezone = "Africa/Niamey";
    }else if (strcmp(country, "NF") == 0) {
        timezone = "Pacific/Norfolk";
    }else if (strcmp(country, "NG") == 0) {
        timezone = "Africa/Lagos";
    }else if (strcmp(country, "NI") == 0) {
        timezone = "America/Managua";
    }else if (strcmp(country, "NL") == 0) {
        timezone = "Europe/Amsterdam";
    }else if (strcmp(country, "NO") == 0) {
        timezone = "Europe/Oslo";
    }else if (strcmp(country, "NP") == 0) {
        timezone = "Asia/Kathmandu";
    }else if (strcmp(country, "NR") == 0) {
        timezone = "Pacific/Nauru";
    }else if (strcmp(country, "NU") == 0) {
        timezone = "Pacific/Niue";
    }else if (strcmp(country, "NZ") == 0) {
        if (strcmp(region, "85") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "E7") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "E8") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "E9") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F1") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F2") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F3") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F4") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F5") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F6") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F7") == 0) {
            timezone = "Pacific/Chatham";
        }else if (strcmp(region, "F8") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "F9") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "G1") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "G2") == 0) {
            timezone = "Pacific/Auckland";
        }else if (strcmp(region, "G3") == 0) {
            timezone = "Pacific/Auckland";
        }
    }else if (strcmp(country, "OM") == 0) {
        timezone = "Asia/Muscat";
    }else if (strcmp(country, "PA") == 0) {
        timezone = "America/Panama";
    }else if (strcmp(country, "PE") == 0) {
        timezone = "America/Lima";
    }else if (strcmp(country, "PF") == 0) {
        timezone = "Pacific/Marquesas";
    }else if (strcmp(country, "PG") == 0) {
        timezone = "Pacific/Port_Moresby";
    }else if (strcmp(country, "PH") == 0) {
        timezone = "Asia/Manila";
    }else if (strcmp(country, "PK") == 0) {
        timezone = "Asia/Karachi";
    }else if (strcmp(country, "PL") == 0) {
        timezone = "Europe/Warsaw";
    }else if (strcmp(country, "PM") == 0) {
        timezone = "America/Miquelon";
    }else if (strcmp(country, "PN") == 0) {
        timezone = "Pacific/Pitcairn";
    }else if (strcmp(country, "PR") == 0) {
        timezone = "America/Puerto_Rico";
    }else if (strcmp(country, "PS") == 0) {
        timezone = "Asia/Gaza";
    }else if (strcmp(country, "PT") == 0) {
        if (strcmp(region, "02") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Atlantic/Madeira";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "18") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "19") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "22") == 0) {
            timezone = "Europe/Lisbon";
        }else if (strcmp(region, "23") == 0) {
            timezone = "Atlantic/Azores";
        }
    }else if (strcmp(country, "PW") == 0) {
        timezone = "Pacific/Palau";
    }else if (strcmp(country, "PY") == 0) {
        timezone = "America/Asuncion";
    }else if (strcmp(country, "QA") == 0) {
        timezone = "Asia/Qatar";
    }else if (strcmp(country, "RE") == 0) {
        timezone = "Indian/Reunion";
    }else if (strcmp(country, "RO") == 0) {
        timezone = "Europe/Bucharest";
    }else if (strcmp(country, "RS") == 0) {
        timezone = "Europe/Belgrade";
    }else if (strcmp(country, "RU") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Novokuznetsk";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Asia/Novosibirsk";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Vladivostok";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Asia/Anadyr";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "18") == 0) {
            timezone = "Asia/Krasnoyarsk";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "22") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "23") == 0) {
            timezone = "Europe/Kaliningrad";
        }else if (strcmp(region, "24") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "25") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "26") == 0) {
            timezone = "Asia/Kamchatka";
        }else if (strcmp(region, "27") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "28") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "29") == 0) {
            timezone = "Asia/Novokuznetsk";
        }else if (strcmp(region, "30") == 0) {
            timezone = "Asia/Vladivostok";
        }else if (strcmp(region, "31") == 0) {
            timezone = "Asia/Krasnoyarsk";
        }else if (strcmp(region, "32") == 0) {
            timezone = "Asia/Omsk";
        }else if (strcmp(region, "33") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "34") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "35") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "36") == 0) {
            timezone = "Asia/Anadyr";
        }else if (strcmp(region, "37") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "38") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "39") == 0) {
            timezone = "Asia/Krasnoyarsk";
        }else if (strcmp(region, "40") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "41") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "42") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "43") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "44") == 0) {
            timezone = "Asia/Magadan";
        }else if (strcmp(region, "45") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "46") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "47") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "48") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "49") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "50") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "51") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "52") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "53") == 0) {
            timezone = "Asia/Novosibirsk";
        }else if (strcmp(region, "54") == 0) {
            timezone = "Asia/Omsk";
        }else if (strcmp(region, "55") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "56") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "57") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "58") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "59") == 0) {
            timezone = "Asia/Vladivostok";
        }else if (strcmp(region, "60") == 0) {
            timezone = "Europe/Kaliningrad";
        }else if (strcmp(region, "61") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "62") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "63") == 0) {
            timezone = "Asia/Yakutsk";
        }else if (strcmp(region, "64") == 0) {
            timezone = "Asia/Sakhalin";
        }else if (strcmp(region, "65") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "66") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "67") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "68") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "69") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "70") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "71") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "72") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "73") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "74") == 0) {
            timezone = "Asia/Krasnoyarsk";
        }else if (strcmp(region, "75") == 0) {
            timezone = "Asia/Novosibirsk";
        }else if (strcmp(region, "76") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "77") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "78") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "79") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "80") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "81") == 0) {
            timezone = "Europe/Samara";
        }else if (strcmp(region, "82") == 0) {
            timezone = "Asia/Irkutsk";
        }else if (strcmp(region, "83") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "84") == 0) {
            timezone = "Europe/Volgograd";
        }else if (strcmp(region, "85") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "86") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "87") == 0) {
            timezone = "Asia/Novosibirsk";
        }else if (strcmp(region, "88") == 0) {
            timezone = "Europe/Moscow";
        }else if (strcmp(region, "89") == 0) {
            timezone = "Asia/Vladivostok";
        }else if (strcmp(region, "90") == 0) {
            timezone = "Asia/Yekaterinburg";
        }else if (strcmp(region, "91") == 0) {
            timezone = "Asia/Krasnoyarsk";
        }else if (strcmp(region, "92") == 0) {
            timezone = "Asia/Anadyr";
        }else if (strcmp(region, "93") == 0) {
            timezone = "Asia/Irkutsk";
        }
    }else if (strcmp(country, "RW") == 0) {
        timezone = "Africa/Kigali";
    }else if (strcmp(country, "SA") == 0) {
        timezone = "Asia/Riyadh";
    }else if (strcmp(country, "SB") == 0) {
        timezone = "Pacific/Guadalcanal";
    }else if (strcmp(country, "SC") == 0) {
        timezone = "Indian/Mahe";
    }else if (strcmp(country, "SD") == 0) {
        timezone = "Africa/Khartoum";
    }else if (strcmp(country, "SE") == 0) {
        timezone = "Europe/Stockholm";
    }else if (strcmp(country, "SG") == 0) {
        timezone = "Asia/Singapore";
    }else if (strcmp(country, "SH") == 0) {
        timezone = "Atlantic/St_Helena";
    }else if (strcmp(country, "SI") == 0) {
        timezone = "Europe/Ljubljana";
    }else if (strcmp(country, "SJ") == 0) {
        timezone = "Arctic/Longyearbyen";
    }else if (strcmp(country, "SK") == 0) {
        timezone = "Europe/Bratislava";
    }else if (strcmp(country, "SL") == 0) {
        timezone = "Africa/Freetown";
    }else if (strcmp(country, "SM") == 0) {
        timezone = "Europe/San_Marino";
    }else if (strcmp(country, "SN") == 0) {
        timezone = "Africa/Dakar";
    }else if (strcmp(country, "SO") == 0) {
        timezone = "Africa/Mogadishu";
    }else if (strcmp(country, "SR") == 0) {
        timezone = "America/Paramaribo";
    }else if (strcmp(country, "SS") == 0) {
        timezone = "Africa/Juba";
    }else if (strcmp(country, "ST") == 0) {
        timezone = "Africa/Sao_Tome";
    }else if (strcmp(country, "SV") == 0) {
        timezone = "America/El_Salvador";
    }else if (strcmp(country, "SX") == 0) {
        timezone = "America/Curacao";
    }else if (strcmp(country, "SY") == 0) {
        timezone = "Asia/Damascus";
    }else if (strcmp(country, "SZ") == 0) {
        timezone = "Africa/Mbabane";
    }else if (strcmp(country, "TC") == 0) {
        timezone = "America/Grand_Turk";
    }else if (strcmp(country, "TD") == 0) {
        timezone = "Africa/Ndjamena";
    }else if (strcmp(country, "TF") == 0) {
        timezone = "Indian/Kerguelen";
    }else if (strcmp(country, "TG") == 0) {
        timezone = "Africa/Lome";
    }else if (strcmp(country, "TH") == 0) {
        timezone = "Asia/Bangkok";
    }else if (strcmp(country, "TJ") == 0) {
        timezone = "Asia/Dushanbe";
    }else if (strcmp(country, "TK") == 0) {
        timezone = "Pacific/Fakaofo";
    }else if (strcmp(country, "TL") == 0) {
        timezone = "Asia/Dili";
    }else if (strcmp(country, "TM") == 0) {
        timezone = "Asia/Ashgabat";
    }else if (strcmp(country, "TN") == 0) {
        timezone = "Africa/Tunis";
    }else if (strcmp(country, "TO") == 0) {
        timezone = "Pacific/Tongatapu";
    }else if (strcmp(country, "TR") == 0) {
        timezone = "Asia/Istanbul";
    }else if (strcmp(country, "TT") == 0) {
        timezone = "America/Port_of_Spain";
    }else if (strcmp(country, "TV") == 0) {
        timezone = "Pacific/Funafuti";
    }else if (strcmp(country, "TW") == 0) {
        timezone = "Asia/Taipei";
    }else if (strcmp(country, "TZ") == 0) {
        timezone = "Africa/Dar_es_Salaam";
    }else if (strcmp(country, "UA") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "04") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Europe/Simferopol";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "11") == 0) {
            timezone = "Europe/Simferopol";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "15") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "16") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "17") == 0) {
            timezone = "Europe/Simferopol";
        }else if (strcmp(region, "18") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "19") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "20") == 0) {
            timezone = "Europe/Simferopol";
        }else if (strcmp(region, "21") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "22") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "23") == 0) {
            timezone = "Europe/Kiev";
        }else if (strcmp(region, "24") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "25") == 0) {
            timezone = "Europe/Uzhgorod";
        }else if (strcmp(region, "26") == 0) {
            timezone = "Europe/Zaporozhye";
        }else if (strcmp(region, "27") == 0) {
            timezone = "Europe/Kiev";
        }
    }else if (strcmp(country, "UG") == 0) {
        timezone = "Africa/Kampala";
    }else if (strcmp(country, "UM") == 0) {
        timezone = "Pacific/Wake";
    }else if (strcmp(country, "US") == 0) {
        if (strcmp(region, "AK") == 0) {
            timezone = "America/Anchorage";
        }else if (strcmp(region, "AL") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "AR") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "AZ") == 0) {
            timezone = "America/Phoenix";
        }else if (strcmp(region, "CA") == 0) {
            timezone = "America/Los_Angeles";
        }else if (strcmp(region, "CO") == 0) {
            timezone = "America/Denver";
        }else if (strcmp(region, "CT") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "DC") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "DE") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "FL") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "GA") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "HI") == 0) {
            timezone = "Pacific/Honolulu";
        }else if (strcmp(region, "IA") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "ID") == 0) {
            timezone = "America/Denver";
        }else if (strcmp(region, "IL") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "IN") == 0) {
            timezone = "America/Indiana/Indianapolis";
        }else if (strcmp(region, "KS") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "KY") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "LA") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "MA") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "MD") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "ME") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "MI") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "MN") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "MO") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "MS") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "MT") == 0) {
            timezone = "America/Denver";
        }else if (strcmp(region, "NC") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "ND") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "NE") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "NH") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "NJ") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "NM") == 0) {
            timezone = "America/Denver";
        }else if (strcmp(region, "NV") == 0) {
            timezone = "America/Los_Angeles";
        }else if (strcmp(region, "NY") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "OH") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "OK") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "OR") == 0) {
            timezone = "America/Los_Angeles";
        }else if (strcmp(region, "PA") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "RI") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "SC") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "SD") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "TN") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "TX") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "UT") == 0) {
            timezone = "America/Denver";
        }else if (strcmp(region, "VA") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "VT") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "WA") == 0) {
            timezone = "America/Los_Angeles";
        }else if (strcmp(region, "WI") == 0) {
            timezone = "America/Chicago";
        }else if (strcmp(region, "WV") == 0) {
            timezone = "America/New_York";
        }else if (strcmp(region, "WY") == 0) {
            timezone = "America/Denver";
        }
    }else if (strcmp(country, "UY") == 0) {
        timezone = "America/Montevideo";
    }else if (strcmp(country, "UZ") == 0) {
        if (strcmp(region, "01") == 0) {
            timezone = "Asia/Tashkent";
        }else if (strcmp(region, "02") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "03") == 0) {
            timezone = "Asia/Tashkent";
        }else if (strcmp(region, "05") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "06") == 0) {
            timezone = "Asia/Tashkent";
        }else if (strcmp(region, "07") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "08") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "09") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "10") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "12") == 0) {
            timezone = "Asia/Samarkand";
        }else if (strcmp(region, "13") == 0) {
            timezone = "Asia/Tashkent";
        }else if (strcmp(region, "14") == 0) {
            timezone = "Asia/Tashkent";
        }
    }else if (strcmp(country, "VA") == 0) {
        timezone = "Europe/Vatican";
    }else if (strcmp(country, "VC") == 0) {
        timezone = "America/St_Vincent";
    }else if (strcmp(country, "VE") == 0) {
        timezone = "America/Caracas";
    }else if (strcmp(country, "VG") == 0) {
        timezone = "America/Tortola";
    }else if (strcmp(country, "VI") == 0) {
        timezone = "America/St_Thomas";
    }else if (strcmp(country, "VN") == 0) {
        timezone = "Asia/Phnom_Penh";
    }else if (strcmp(country, "VU") == 0) {
        timezone = "Pacific/Efate";
    }else if (strcmp(country, "WF") == 0) {
        timezone = "Pacific/Wallis";
    }else if (strcmp(country, "WS") == 0) {
        timezone = "Pacific/Pago_Pago";
    }else if (strcmp(country, "YE") == 0) {
        timezone = "Asia/Aden";
    }else if (strcmp(country, "YT") == 0) {
        timezone = "Indian/Mayotte";
    }else if (strcmp(country, "YU") == 0) {
        timezone = "Europe/Belgrade";
    }else if (strcmp(country, "ZA") == 0) {
        timezone = "Africa/Johannesburg";
    }else if (strcmp(country, "ZM") == 0) {
        timezone = "Africa/Lusaka";
    }else if (strcmp(country, "ZW") == 0) {
        timezone = "Africa/Harare";
    }
    return timezone;
}
