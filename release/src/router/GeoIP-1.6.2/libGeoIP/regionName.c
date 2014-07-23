#include "GeoIP.h"

#include <string.h>
#include <stdio.h>

const char * GeoIP_region_name_by_code(const char * country_code,
                                       const char * region_code)
{
    const char * name = NULL;
    int region_code2 = -1;
    if (region_code == NULL) {
        return NULL;
    }

    if (   ((region_code[0] >= 48) && (region_code[0] < (48 + 10)))
           && ((region_code[1] >= 48) && (region_code[1] < (48 + 10)))
           ) {

        /* only numbers, that shorten the large switch statements */
        region_code2 = (region_code[0] - 48) * 10 + region_code[1] - 48;
    }else if (    (    ((region_code[0] >= 65) && (region_code[0] < (65 + 26)))
                       || ((region_code[0] >= 48) &&
                           (region_code[0] < (48 + 10))))
                  && (    ((region_code[1] >= 65) &&
                           (region_code[1] < (65 + 26)))
                          || ((region_code[1] >= 48) &&
                              (region_code[1] < (48 + 10))))
                  ) {

        region_code2 =
            (region_code[0] - 48) * (65 + 26 - 48) + region_code[1] - 48 + 100;
    }

    if (region_code2 == -1) {
        return NULL;
    }

    if (strcmp(country_code, "CA") == 0) {
        switch (region_code2) {
        case 849:
            return "Alberta";
        case 893:
            return "British Columbia";
        case 1365:
            return "Manitoba";
        case 1408:
            return "New Brunswick";
        case 1418:
            return "Newfoundland";
        case 1425:
            return "Nova Scotia";
        case 1427:
            return "Nunavut";
        case 1463:
            return "Ontario";
        case 1497:
            return "Prince Edward Island";
        case 1538:
            return "Quebec";
        case 1632:
            return "Saskatchewan";
        case 1426:
            return "Northwest Territories";
        case 1899:
            return "Yukon Territory";
        }
    }else if (strcmp(country_code, "US") == 0) {
        switch (region_code2) {
        case 848:
            return "Armed Forces Americas";
        case 852:
            return "Armed Forces Europe, Middle East, & Canada";
        case 858:
            return "Alaska";
        case 859:
            return "Alabama";
        case 863:
            return "Armed Forces Pacific";
        case 865:
            return "Arkansas";
        case 866:
            return "American Samoa";
        case 873:
            return "Arizona";
        case 934:
            return "California";
        case 948:
            return "Colorado";
        case 953:
            return "Connecticut";
        case 979:
            return "District of Columbia";
        case 981:
            return "Delaware";
        case 1074:
            return "Florida";
        case 1075:
            return "Federated States of Micronesia";
        case 1106:
            return "Georgia";
        case 1126:
            return "Guam";
        case 1157:
            return "Hawaii";
        case 1192:
            return "Iowa";
        case 1195:
            return "Idaho";
        case 1203:
            return "Illinois";
        case 1205:
            return "Indiana";
        case 1296:
            return "Kansas";
        case 1302:
            return "Kentucky";
        case 1321:
            return "Louisiana";
        case 1364:
            return "Massachusetts";
        case 1367:
            return "Maryland";
        case 1368:
            return "Maine";
        case 1371:
            return "Marshall Islands";
        case 1372:
            return "Michigan";
        case 1377:
            return "Minnesota";
        case 1378:
            return "Missouri";
        case 1379:
            return "Northern Mariana Islands";
        case 1382:
            return "Mississippi";
        case 1383:
            return "Montana";
        case 1409:
            return "North Carolina";
        case 1410:
            return "North Dakota";
        case 1411:
            return "Nebraska";
        case 1414:
            return "New Hampshire";
        case 1416:
            return "New Jersey";
        case 1419:
            return "New Mexico";
        case 1428:
            return "Nevada";
        case 1431:
            return "New York";
        case 1457:
            return "Ohio";
        case 1460:
            return "Oklahoma";
        case 1467:
            return "Oregon";
        case 1493:
            return "Pennsylvania";
        case 1510:
            return "Puerto Rico";
        case 1515:
            return "Palau";
        case 1587:
            return "Rhode Island";
        case 1624:
            return "South Carolina";
        case 1625:
            return "South Dakota";
        case 1678:
            return "Tennessee";
        case 1688:
            return "Texas";
        case 1727:
            return "Utah";
        case 1751:
            return "Virginia";
        case 1759:
            return "Virgin Islands";
        case 1770:
            return "Vermont";
        case 1794:
            return "Washington";
        case 1815:
            return "West Virginia";
        case 1802:
            return "Wisconsin";
        case 1818:
            return "Wyoming";
        }
    }else if (strcmp(country_code, "AD") == 0) {
        switch (region_code2) {
        case 2:
            return "Canillo";
        case 3:
            return "Encamp";
        case 4:
            return "La Massana";
        case 5:
            return "Ordino";
        case 6:
            return "Sant Julia de Loria";
        case 7:
            return "Andorra la Vella";
        case 8:
            return "Escaldes-Engordany";
        }
    }else if (strcmp(country_code, "AE") == 0) {
        switch (region_code2) {
        case 1:
            return "Abu Dhabi";
        case 2:
            return "Ajman";
        case 3:
            return "Dubai";
        case 4:
            return "Fujairah";
        case 5:
            return "Ras Al Khaimah";
        case 6:
            return "Sharjah";
        case 7:
            return "Umm Al Quwain";
        }
    }else if (strcmp(country_code, "AF") == 0) {
        switch (region_code2) {
        case 1:
            return "Badakhshan";
        case 2:
            return "Badghis";
        case 3:
            return "Baghlan";
        case 5:
            return "Bamian";
        case 6:
            return "Farah";
        case 7:
            return "Faryab";
        case 8:
            return "Ghazni";
        case 9:
            return "Ghowr";
        case 10:
            return "Helmand";
        case 11:
            return "Herat";
        case 13:
            return "Kabol";
        case 14:
            return "Kapisa";
        case 17:
            return "Lowgar";
        case 18:
            return "Nangarhar";
        case 19:
            return "Nimruz";
        case 23:
            return "Kandahar";
        case 24:
            return "Kondoz";
        case 26:
            return "Takhar";
        case 27:
            return "Vardak";
        case 28:
            return "Zabol";
        case 29:
            return "Paktika";
        case 30:
            return "Balkh";
        case 31:
            return "Jowzjan";
        case 32:
            return "Samangan";
        case 33:
            return "Sar-e Pol";
        case 34:
            return "Konar";
        case 35:
            return "Laghman";
        case 36:
            return "Paktia";
        case 37:
            return "Khowst";
        case 38:
            return "Nurestan";
        case 39:
            return "Oruzgan";
        case 40:
            return "Parvan";
        case 41:
            return "Daykondi";
        case 42:
            return "Panjshir";
        }
    }else if (strcmp(country_code, "AG") == 0) {
        switch (region_code2) {
        case 1:
            return "Barbuda";
        case 3:
            return "Saint George";
        case 4:
            return "Saint John";
        case 5:
            return "Saint Mary";
        case 6:
            return "Saint Paul";
        case 7:
            return "Saint Peter";
        case 8:
            return "Saint Philip";
        case 9:
            return "Redonda";
        }
    }else if (strcmp(country_code, "AL") == 0) {
        switch (region_code2) {
        case 40:
            return "Berat";
        case 41:
            return "Diber";
        case 42:
            return "Durres";
        case 43:
            return "Elbasan";
        case 44:
            return "Fier";
        case 45:
            return "Gjirokaster";
        case 46:
            return "Korce";
        case 47:
            return "Kukes";
        case 48:
            return "Lezhe";
        case 49:
            return "Shkoder";
        case 50:
            return "Tirane";
        case 51:
            return "Vlore";
        }
    }else if (strcmp(country_code, "AM") == 0) {
        switch (region_code2) {
        case 1:
            return "Aragatsotn";
        case 2:
            return "Ararat";
        case 3:
            return "Armavir";
        case 4:
            return "Geghark'unik'";
        case 5:
            return "Kotayk'";
        case 6:
            return "Lorri";
        case 7:
            return "Shirak";
        case 8:
            return "Syunik'";
        case 9:
            return "Tavush";
        case 10:
            return "Vayots' Dzor";
        case 11:
            return "Yerevan";
        }
    }else if (strcmp(country_code, "AO") == 0) {
        switch (region_code2) {
        case 1:
            return "Benguela";
        case 2:
            return "Bie";
        case 3:
            return "Cabinda";
        case 4:
            return "Cuando Cubango";
        case 5:
            return "Cuanza Norte";
        case 6:
            return "Cuanza Sul";
        case 7:
            return "Cunene";
        case 8:
            return "Huambo";
        case 9:
            return "Huila";
        case 12:
            return "Malanje";
        case 13:
            return "Namibe";
        case 14:
            return "Moxico";
        case 15:
            return "Uige";
        case 16:
            return "Zaire";
        case 17:
            return "Lunda Norte";
        case 18:
            return "Lunda Sul";
        case 19:
            return "Bengo";
        case 20:
            return "Luanda";
        }
    }else if (strcmp(country_code, "AR") == 0) {
        switch (region_code2) {
        case 1:
            return "Buenos Aires";
        case 2:
            return "Catamarca";
        case 3:
            return "Chaco";
        case 4:
            return "Chubut";
        case 5:
            return "Cordoba";
        case 6:
            return "Corrientes";
        case 7:
            return "Distrito Federal";
        case 8:
            return "Entre Rios";
        case 9:
            return "Formosa";
        case 10:
            return "Jujuy";
        case 11:
            return "La Pampa";
        case 12:
            return "La Rioja";
        case 13:
            return "Mendoza";
        case 14:
            return "Misiones";
        case 15:
            return "Neuquen";
        case 16:
            return "Rio Negro";
        case 17:
            return "Salta";
        case 18:
            return "San Juan";
        case 19:
            return "San Luis";
        case 20:
            return "Santa Cruz";
        case 21:
            return "Santa Fe";
        case 22:
            return "Santiago del Estero";
        case 23:
            return "Tierra del Fuego";
        case 24:
            return "Tucuman";
        }
    }else if (strcmp(country_code, "AT") == 0) {
        switch (region_code2) {
        case 1:
            return "Burgenland";
        case 2:
            return "Karnten";
        case 3:
            return "Niederosterreich";
        case 4:
            return "Oberosterreich";
        case 5:
            return "Salzburg";
        case 6:
            return "Steiermark";
        case 7:
            return "Tirol";
        case 8:
            return "Vorarlberg";
        case 9:
            return "Wien";
        }
    }else if (strcmp(country_code, "AU") == 0) {
        switch (region_code2) {
        case 1:
            return "Australian Capital Territory";
        case 2:
            return "New South Wales";
        case 3:
            return "Northern Territory";
        case 4:
            return "Queensland";
        case 5:
            return "South Australia";
        case 6:
            return "Tasmania";
        case 7:
            return "Victoria";
        case 8:
            return "Western Australia";
        }
    }else if (strcmp(country_code, "AZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Abseron";
        case 2:
            return "Agcabadi";
        case 3:
            return "Agdam";
        case 4:
            return "Agdas";
        case 5:
            return "Agstafa";
        case 6:
            return "Agsu";
        case 7:
            return "Ali Bayramli";
        case 8:
            return "Astara";
        case 9:
            return "Baki";
        case 10:
            return "Balakan";
        case 11:
            return "Barda";
        case 12:
            return "Beylaqan";
        case 13:
            return "Bilasuvar";
        case 14:
            return "Cabrayil";
        case 15:
            return "Calilabad";
        case 16:
            return "Daskasan";
        case 17:
            return "Davaci";
        case 18:
            return "Fuzuli";
        case 19:
            return "Gadabay";
        case 20:
            return "Ganca";
        case 21:
            return "Goranboy";
        case 22:
            return "Goycay";
        case 23:
            return "Haciqabul";
        case 24:
            return "Imisli";
        case 25:
            return "Ismayilli";
        case 26:
            return "Kalbacar";
        case 27:
            return "Kurdamir";
        case 28:
            return "Lacin";
        case 29:
            return "Lankaran";
        case 30:
            return "Lankaran";
        case 31:
            return "Lerik";
        case 32:
            return "Masalli";
        case 33:
            return "Mingacevir";
        case 34:
            return "Naftalan";
        case 35:
            return "Naxcivan";
        case 36:
            return "Neftcala";
        case 37:
            return "Oguz";
        case 38:
            return "Qabala";
        case 39:
            return "Qax";
        case 40:
            return "Qazax";
        case 41:
            return "Qobustan";
        case 42:
            return "Quba";
        case 43:
            return "Qubadli";
        case 44:
            return "Qusar";
        case 45:
            return "Saatli";
        case 46:
            return "Sabirabad";
        case 47:
            return "Saki";
        case 48:
            return "Saki";
        case 49:
            return "Salyan";
        case 50:
            return "Samaxi";
        case 51:
            return "Samkir";
        case 52:
            return "Samux";
        case 53:
            return "Siyazan";
        case 54:
            return "Sumqayit";
        case 55:
            return "Susa";
        case 56:
            return "Susa";
        case 57:
            return "Tartar";
        case 58:
            return "Tovuz";
        case 59:
            return "Ucar";
        case 60:
            return "Xacmaz";
        case 61:
            return "Xankandi";
        case 62:
            return "Xanlar";
        case 63:
            return "Xizi";
        case 64:
            return "Xocali";
        case 65:
            return "Xocavand";
        case 66:
            return "Yardimli";
        case 67:
            return "Yevlax";
        case 68:
            return "Yevlax";
        case 69:
            return "Zangilan";
        case 70:
            return "Zaqatala";
        case 71:
            return "Zardab";
        }
    }else if (strcmp(country_code, "BA") == 0) {
        switch (region_code2) {
        case 1:
            return "Federation of Bosnia and Herzegovina";
        case 3:
            return "Brcko District";
        case 2:
            return "Republika Srpska";
        }
    }else if (strcmp(country_code, "BB") == 0) {
        switch (region_code2) {
        case 1:
            return "Christ Church";
        case 2:
            return "Saint Andrew";
        case 3:
            return "Saint George";
        case 4:
            return "Saint James";
        case 5:
            return "Saint John";
        case 6:
            return "Saint Joseph";
        case 7:
            return "Saint Lucy";
        case 8:
            return "Saint Michael";
        case 9:
            return "Saint Peter";
        case 10:
            return "Saint Philip";
        case 11:
            return "Saint Thomas";
        }
    }else if (strcmp(country_code, "BD") == 0) {
        switch (region_code2) {
        case 81:
            return "Dhaka";
        case 82:
            return "Khulna";
        case 83:
            return "Rajshahi";
        case 84:
            return "Chittagong";
        case 85:
            return "Barisal";
        case 86:
            return "Sylhet";
        case 87:
            return "Rangpur";
        }
    }else if (strcmp(country_code, "BE") == 0) {
        switch (region_code2) {
        case 1:
            return "Antwerpen";
        case 3:
            return "Hainaut";
        case 4:
            return "Liege";
        case 5:
            return "Limburg";
        case 6:
            return "Luxembourg";
        case 7:
            return "Namur";
        case 8:
            return "Oost-Vlaanderen";
        case 9:
            return "West-Vlaanderen";
        case 10:
            return "Brabant Wallon";
        case 11:
            return "Brussels Hoofdstedelijk Gewest";
        case 12:
            return "Vlaams-Brabant";
        case 13:
            return "Flanders";
        case 14:
            return "Wallonia";
        }
    }else if (strcmp(country_code, "BF") == 0) {
        switch (region_code2) {
        case 15:
            return "Bam";
        case 19:
            return "Boulkiemde";
        case 20:
            return "Ganzourgou";
        case 21:
            return "Gnagna";
        case 28:
            return "Kouritenga";
        case 33:
            return "Oudalan";
        case 34:
            return "Passore";
        case 36:
            return "Sanguie";
        case 40:
            return "Soum";
        case 42:
            return "Tapoa";
        case 44:
            return "Zoundweogo";
        case 45:
            return "Bale";
        case 46:
            return "Banwa";
        case 47:
            return "Bazega";
        case 48:
            return "Bougouriba";
        case 49:
            return "Boulgou";
        case 50:
            return "Gourma";
        case 51:
            return "Houet";
        case 52:
            return "Ioba";
        case 53:
            return "Kadiogo";
        case 54:
            return "Kenedougou";
        case 55:
            return "Komoe";
        case 56:
            return "Komondjari";
        case 57:
            return "Kompienga";
        case 58:
            return "Kossi";
        case 59:
            return "Koulpelogo";
        case 60:
            return "Kourweogo";
        case 61:
            return "Leraba";
        case 62:
            return "Loroum";
        case 63:
            return "Mouhoun";
        case 64:
            return "Namentenga";
        case 65:
            return "Naouri";
        case 66:
            return "Nayala";
        case 67:
            return "Noumbiel";
        case 68:
            return "Oubritenga";
        case 69:
            return "Poni";
        case 70:
            return "Sanmatenga";
        case 71:
            return "Seno";
        case 72:
            return "Sissili";
        case 73:
            return "Sourou";
        case 74:
            return "Tuy";
        case 75:
            return "Yagha";
        case 76:
            return "Yatenga";
        case 77:
            return "Ziro";
        case 78:
            return "Zondoma";
        }
    }else if (strcmp(country_code, "BG") == 0) {
        switch (region_code2) {
        case 33:
            return "Mikhaylovgrad";
        case 38:
            return "Blagoevgrad";
        case 39:
            return "Burgas";
        case 40:
            return "Dobrich";
        case 41:
            return "Gabrovo";
        case 42:
            return "Grad Sofiya";
        case 43:
            return "Khaskovo";
        case 44:
            return "Kurdzhali";
        case 45:
            return "Kyustendil";
        case 46:
            return "Lovech";
        case 47:
            return "Montana";
        case 48:
            return "Pazardzhik";
        case 49:
            return "Pernik";
        case 50:
            return "Pleven";
        case 51:
            return "Plovdiv";
        case 52:
            return "Razgrad";
        case 53:
            return "Ruse";
        case 54:
            return "Shumen";
        case 55:
            return "Silistra";
        case 56:
            return "Sliven";
        case 57:
            return "Smolyan";
        case 58:
            return "Sofiya";
        case 59:
            return "Stara Zagora";
        case 60:
            return "Turgovishte";
        case 61:
            return "Varna";
        case 62:
            return "Veliko Turnovo";
        case 63:
            return "Vidin";
        case 64:
            return "Vratsa";
        case 65:
            return "Yambol";
        }
    }else if (strcmp(country_code, "BH") == 0) {
        switch (region_code2) {
        case 1:
            return "Al Hadd";
        case 2:
            return "Al Manamah";
        case 5:
            return "Jidd Hafs";
        case 6:
            return "Sitrah";
        case 8:
            return "Al Mintaqah al Gharbiyah";
        case 9:
            return "Mintaqat Juzur Hawar";
        case 10:
            return "Al Mintaqah ash Shamaliyah";
        case 11:
            return "Al Mintaqah al Wusta";
        case 12:
            return "Madinat";
        case 13:
            return "Ar Rifa";
        case 14:
            return "Madinat Hamad";
        case 15:
            return "Al Muharraq";
        case 16:
            return "Al Asimah";
        case 17:
            return "Al Janubiyah";
        case 18:
            return "Ash Shamaliyah";
        case 19:
            return "Al Wusta";
        }
    }else if (strcmp(country_code, "BI") == 0) {
        switch (region_code2) {
        case 2:
            return "Bujumbura";
        case 9:
            return "Bubanza";
        case 10:
            return "Bururi";
        case 11:
            return "Cankuzo";
        case 12:
            return "Cibitoke";
        case 13:
            return "Gitega";
        case 14:
            return "Karuzi";
        case 15:
            return "Kayanza";
        case 16:
            return "Kirundo";
        case 17:
            return "Makamba";
        case 18:
            return "Muyinga";
        case 19:
            return "Ngozi";
        case 20:
            return "Rutana";
        case 21:
            return "Ruyigi";
        case 22:
            return "Muramvya";
        case 23:
            return "Mwaro";
        }
    }else if (strcmp(country_code, "BJ") == 0) {
        switch (region_code2) {
        case 7:
            return "Alibori";
        case 8:
            return "Atakora";
        case 9:
            return "Atlanyique";
        case 10:
            return "Borgou";
        case 11:
            return "Collines";
        case 12:
            return "Kouffo";
        case 13:
            return "Donga";
        case 14:
            return "Littoral";
        case 15:
            return "Mono";
        case 16:
            return "Oueme";
        case 17:
            return "Plateau";
        case 18:
            return "Zou";
        }
    }else if (strcmp(country_code, "BM") == 0) {
        switch (region_code2) {
        case 1:
            return "Devonshire";
        case 2:
            return "Hamilton";
        case 3:
            return "Hamilton";
        case 4:
            return "Paget";
        case 5:
            return "Pembroke";
        case 6:
            return "Saint George";
        case 7:
            return "Saint George's";
        case 8:
            return "Sandys";
        case 9:
            return "Smiths";
        case 10:
            return "Southampton";
        case 11:
            return "Warwick";
        }
    }else if (strcmp(country_code, "BN") == 0) {
        switch (region_code2) {
        case 7:
            return "Alibori";
        case 8:
            return "Belait";
        case 9:
            return "Brunei and Muara";
        case 10:
            return "Temburong";
        case 11:
            return "Collines";
        case 12:
            return "Kouffo";
        case 13:
            return "Donga";
        case 14:
            return "Littoral";
        case 15:
            return "Tutong";
        case 16:
            return "Oueme";
        case 17:
            return "Plateau";
        case 18:
            return "Zou";
        }
    }else if (strcmp(country_code, "BO") == 0) {
        switch (region_code2) {
        case 1:
            return "Chuquisaca";
        case 2:
            return "Cochabamba";
        case 3:
            return "El Beni";
        case 4:
            return "La Paz";
        case 5:
            return "Oruro";
        case 6:
            return "Pando";
        case 7:
            return "Potosi";
        case 8:
            return "Santa Cruz";
        case 9:
            return "Tarija";
        }
    }else if (strcmp(country_code, "BR") == 0) {
        switch (region_code2) {
        case 1:
            return "Acre";
        case 2:
            return "Alagoas";
        case 3:
            return "Amapa";
        case 4:
            return "Amazonas";
        case 5:
            return "Bahia";
        case 6:
            return "Ceara";
        case 7:
            return "Distrito Federal";
        case 8:
            return "Espirito Santo";
        case 11:
            return "Mato Grosso do Sul";
        case 13:
            return "Maranhao";
        case 14:
            return "Mato Grosso";
        case 15:
            return "Minas Gerais";
        case 16:
            return "Para";
        case 17:
            return "Paraiba";
        case 18:
            return "Parana";
        case 20:
            return "Piaui";
        case 21:
            return "Rio de Janeiro";
        case 22:
            return "Rio Grande do Norte";
        case 23:
            return "Rio Grande do Sul";
        case 24:
            return "Rondonia";
        case 25:
            return "Roraima";
        case 26:
            return "Santa Catarina";
        case 27:
            return "Sao Paulo";
        case 28:
            return "Sergipe";
        case 29:
            return "Goias";
        case 30:
            return "Pernambuco";
        case 31:
            return "Tocantins";
        }
    }else if (strcmp(country_code, "BS") == 0) {
        switch (region_code2) {
        case 5:
            return "Bimini";
        case 6:
            return "Cat Island";
        case 10:
            return "Exuma";
        case 13:
            return "Inagua";
        case 15:
            return "Long Island";
        case 16:
            return "Mayaguana";
        case 18:
            return "Ragged Island";
        case 22:
            return "Harbour Island";
        case 23:
            return "New Providence";
        case 24:
            return "Acklins and Crooked Islands";
        case 25:
            return "Freeport";
        case 26:
            return "Fresh Creek";
        case 27:
            return "Governor's Harbour";
        case 28:
            return "Green Turtle Cay";
        case 29:
            return "High Rock";
        case 30:
            return "Kemps Bay";
        case 31:
            return "Marsh Harbour";
        case 32:
            return "Nichollstown and Berry Islands";
        case 33:
            return "Rock Sound";
        case 34:
            return "Sandy Point";
        case 35:
            return "San Salvador and Rum Cay";
        }
    }else if (strcmp(country_code, "BT") == 0) {
        switch (region_code2) {
        case 5:
            return "Bumthang";
        case 6:
            return "Chhukha";
        case 7:
            return "Chirang";
        case 8:
            return "Daga";
        case 9:
            return "Geylegphug";
        case 10:
            return "Ha";
        case 11:
            return "Lhuntshi";
        case 12:
            return "Mongar";
        case 13:
            return "Paro";
        case 14:
            return "Pemagatsel";
        case 15:
            return "Punakha";
        case 16:
            return "Samchi";
        case 17:
            return "Samdrup";
        case 18:
            return "Shemgang";
        case 19:
            return "Tashigang";
        case 20:
            return "Thimphu";
        case 21:
            return "Tongsa";
        case 22:
            return "Wangdi Phodrang";
        }
    }else if (strcmp(country_code, "BW") == 0) {
        switch (region_code2) {
        case 1:
            return "Central";
        case 3:
            return "Ghanzi";
        case 4:
            return "Kgalagadi";
        case 5:
            return "Kgatleng";
        case 6:
            return "Kweneng";
        case 8:
            return "North-East";
        case 9:
            return "South-East";
        case 10:
            return "Southern";
        case 11:
            return "North-West";
        }
    }else if (strcmp(country_code, "BY") == 0) {
        switch (region_code2) {
        case 1:
            return "Brestskaya Voblasts'";
        case 2:
            return "Homyel'skaya Voblasts'";
        case 3:
            return "Hrodzyenskaya Voblasts'";
        case 4:
            return "Minsk";
        case 5:
            return "Minskaya Voblasts'";
        case 6:
            return "Mahilyowskaya Voblasts'";
        case 7:
            return "Vitsyebskaya Voblasts'";
        }
    }else if (strcmp(country_code, "BZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Belize";
        case 2:
            return "Cayo";
        case 3:
            return "Corozal";
        case 4:
            return "Orange Walk";
        case 5:
            return "Stann Creek";
        case 6:
            return "Toledo";
        }
    }else if (strcmp(country_code, "CD") == 0) {
        switch (region_code2) {
        case 1:
            return "Bandundu";
        case 2:
            return "Equateur";
        case 4:
            return "Kasai-Oriental";
        case 5:
            return "Katanga";
        case 6:
            return "Kinshasa";
        case 8:
            return "Bas-Congo";
        case 9:
            return "Orientale";
        case 10:
            return "Maniema";
        case 11:
            return "Nord-Kivu";
        case 12:
            return "Sud-Kivu";
        }
    }else if (strcmp(country_code, "CF") == 0) {
        switch (region_code2) {
        case 1:
            return "Bamingui-Bangoran";
        case 2:
            return "Basse-Kotto";
        case 3:
            return "Haute-Kotto";
        case 4:
            return "Mambere-Kadei";
        case 5:
            return "Haut-Mbomou";
        case 6:
            return "Kemo";
        case 7:
            return "Lobaye";
        case 8:
            return "Mbomou";
        case 9:
            return "Nana-Mambere";
        case 11:
            return "Ouaka";
        case 12:
            return "Ouham";
        case 13:
            return "Ouham-Pende";
        case 14:
            return "Cuvette-Ouest";
        case 15:
            return "Nana-Grebizi";
        case 16:
            return "Sangha-Mbaere";
        case 17:
            return "Ombella-Mpoko";
        case 18:
            return "Bangui";
        }
    }else if (strcmp(country_code, "CG") == 0) {
        switch (region_code2) {
        case 1:
            return "Bouenza";
        case 4:
            return "Kouilou";
        case 5:
            return "Lekoumou";
        case 6:
            return "Likouala";
        case 7:
            return "Niari";
        case 8:
            return "Plateaux";
        case 10:
            return "Sangha";
        case 11:
            return "Pool";
        case 12:
            return "Brazzaville";
        case 13:
            return "Cuvette";
        case 14:
            return "Cuvette-Ouest";
        }
    }else if (strcmp(country_code, "CH") == 0) {
        switch (region_code2) {
        case 1:
            return "Aargau";
        case 2:
            return "Ausser-Rhoden";
        case 3:
            return "Basel-Landschaft";
        case 4:
            return "Basel-Stadt";
        case 5:
            return "Bern";
        case 6:
            return "Fribourg";
        case 7:
            return "Geneve";
        case 8:
            return "Glarus";
        case 9:
            return "Graubunden";
        case 10:
            return "Inner-Rhoden";
        case 11:
            return "Luzern";
        case 12:
            return "Neuchatel";
        case 13:
            return "Nidwalden";
        case 14:
            return "Obwalden";
        case 15:
            return "Sankt Gallen";
        case 16:
            return "Schaffhausen";
        case 17:
            return "Schwyz";
        case 18:
            return "Solothurn";
        case 19:
            return "Thurgau";
        case 20:
            return "Ticino";
        case 21:
            return "Uri";
        case 22:
            return "Valais";
        case 23:
            return "Vaud";
        case 24:
            return "Zug";
        case 25:
            return "Zurich";
        case 26:
            return "Jura";
        }
    }else if (strcmp(country_code, "CI") == 0) {
        switch (region_code2) {
        case 74:
            return "Agneby";
        case 75:
            return "Bafing";
        case 76:
            return "Bas-Sassandra";
        case 77:
            return "Denguele";
        case 78:
            return "Dix-Huit Montagnes";
        case 79:
            return "Fromager";
        case 80:
            return "Haut-Sassandra";
        case 81:
            return "Lacs";
        case 82:
            return "Lagunes";
        case 83:
            return "Marahoue";
        case 84:
            return "Moyen-Cavally";
        case 85:
            return "Moyen-Comoe";
        case 86:
            return "N'zi-Comoe";
        case 87:
            return "Savanes";
        case 88:
            return "Sud-Bandama";
        case 89:
            return "Sud-Comoe";
        case 90:
            return "Vallee du Bandama";
        case 91:
            return "Worodougou";
        case 92:
            return "Zanzan";
        }
    }else if (strcmp(country_code, "CL") == 0) {
        switch (region_code2) {
        case 1:
            return "Valparaiso";
        case 2:
            return "Aisen del General Carlos Ibanez del Campo";
        case 3:
            return "Antofagasta";
        case 4:
            return "Araucania";
        case 5:
            return "Atacama";
        case 6:
            return "Bio-Bio";
        case 7:
            return "Coquimbo";
        case 8:
            return "Libertador General Bernardo O'Higgins";
        case 9:
            return "Los Lagos";
        case 10:
            return "Magallanes y de la Antartica Chilena";
        case 11:
            return "Maule";
        case 12:
            return "Region Metropolitana";
        case 13:
            return "Tarapaca";
        case 14:
            return "Los Lagos";
        case 15:
            return "Tarapaca";
        case 16:
            return "Arica y Parinacota";
        case 17:
            return "Los Rios";
        }
    }else if (strcmp(country_code, "CM") == 0) {
        switch (region_code2) {
        case 4:
            return "Est";
        case 5:
            return "Littoral";
        case 7:
            return "Nord-Ouest";
        case 8:
            return "Ouest";
        case 9:
            return "Sud-Ouest";
        case 10:
            return "Adamaoua";
        case 11:
            return "Centre";
        case 12:
            return "Extreme-Nord";
        case 13:
            return "Nord";
        case 14:
            return "Sud";
        }
    }else if (strcmp(country_code, "CN") == 0) {
        switch (region_code2) {
        case 1:
            return "Anhui";
        case 2:
            return "Zhejiang";
        case 3:
            return "Jiangxi";
        case 4:
            return "Jiangsu";
        case 5:
            return "Jilin";
        case 6:
            return "Qinghai";
        case 7:
            return "Fujian";
        case 8:
            return "Heilongjiang";
        case 9:
            return "Henan";
        case 10:
            return "Hebei";
        case 11:
            return "Hunan";
        case 12:
            return "Hubei";
        case 13:
            return "Xinjiang";
        case 14:
            return "Xizang";
        case 15:
            return "Gansu";
        case 16:
            return "Guangxi";
        case 18:
            return "Guizhou";
        case 19:
            return "Liaoning";
        case 20:
            return "Nei Mongol";
        case 21:
            return "Ningxia";
        case 22:
            return "Beijing";
        case 23:
            return "Shanghai";
        case 24:
            return "Shanxi";
        case 25:
            return "Shandong";
        case 26:
            return "Shaanxi";
        case 28:
            return "Tianjin";
        case 29:
            return "Yunnan";
        case 30:
            return "Guangdong";
        case 31:
            return "Hainan";
        case 32:
            return "Sichuan";
        case 33:
            return "Chongqing";
        }
    }else if (strcmp(country_code, "CO") == 0) {
        switch (region_code2) {
        case 1:
            return "Amazonas";
        case 2:
            return "Antioquia";
        case 3:
            return "Arauca";
        case 4:
            return "Atlantico";
        case 8:
            return "Caqueta";
        case 9:
            return "Cauca";
        case 10:
            return "Cesar";
        case 11:
            return "Choco";
        case 12:
            return "Cordoba";
        case 14:
            return "Guaviare";
        case 15:
            return "Guainia";
        case 16:
            return "Huila";
        case 17:
            return "La Guajira";
        case 19:
            return "Meta";
        case 20:
            return "Narino";
        case 21:
            return "Norte de Santander";
        case 22:
            return "Putumayo";
        case 23:
            return "Quindio";
        case 24:
            return "Risaralda";
        case 25:
            return "San Andres y Providencia";
        case 26:
            return "Santander";
        case 27:
            return "Sucre";
        case 28:
            return "Tolima";
        case 29:
            return "Valle del Cauca";
        case 30:
            return "Vaupes";
        case 31:
            return "Vichada";
        case 32:
            return "Casanare";
        case 33:
            return "Cundinamarca";
        case 34:
            return "Distrito Especial";
        case 35:
            return "Bolivar";
        case 36:
            return "Boyaca";
        case 37:
            return "Caldas";
        case 38:
            return "Magdalena";
        }
    }else if (strcmp(country_code, "CR") == 0) {
        switch (region_code2) {
        case 1:
            return "Alajuela";
        case 2:
            return "Cartago";
        case 3:
            return "Guanacaste";
        case 4:
            return "Heredia";
        case 6:
            return "Limon";
        case 7:
            return "Puntarenas";
        case 8:
            return "San Jose";
        }
    }else if (strcmp(country_code, "CU") == 0) {
        switch (region_code2) {
        case 1:
            return "Pinar del Rio";
        case 2:
            return "Ciudad de la Habana";
        case 3:
            return "Matanzas";
        case 4:
            return "Isla de la Juventud";
        case 5:
            return "Camaguey";
        case 7:
            return "Ciego de Avila";
        case 8:
            return "Cienfuegos";
        case 9:
            return "Granma";
        case 10:
            return "Guantanamo";
        case 11:
            return "La Habana";
        case 12:
            return "Holguin";
        case 13:
            return "Las Tunas";
        case 14:
            return "Sancti Spiritus";
        case 15:
            return "Santiago de Cuba";
        case 16:
            return "Villa Clara";
        }
    }else if (strcmp(country_code, "CV") == 0) {
        switch (region_code2) {
        case 1:
            return "Boa Vista";
        case 2:
            return "Brava";
        case 4:
            return "Maio";
        case 5:
            return "Paul";
        case 7:
            return "Ribeira Grande";
        case 8:
            return "Sal";
        case 10:
            return "Sao Nicolau";
        case 11:
            return "Sao Vicente";
        case 13:
            return "Mosteiros";
        case 14:
            return "Praia";
        case 15:
            return "Santa Catarina";
        case 16:
            return "Santa Cruz";
        case 17:
            return "Sao Domingos";
        case 18:
            return "Sao Filipe";
        case 19:
            return "Sao Miguel";
        case 20:
            return "Tarrafal";
        }
    }else if (strcmp(country_code, "CY") == 0) {
        switch (region_code2) {
        case 1:
            return "Famagusta";
        case 2:
            return "Kyrenia";
        case 3:
            return "Larnaca";
        case 4:
            return "Nicosia";
        case 5:
            return "Limassol";
        case 6:
            return "Paphos";
        }
    }else if (strcmp(country_code, "CZ") == 0) {
        switch (region_code2) {
        case 52:
            return "Hlavni mesto Praha";
        case 78:
            return "Jihomoravsky kraj";
        case 79:
            return "Jihocesky kraj";
        case 80:
            return "Vysocina";
        case 81:
            return "Karlovarsky kraj";
        case 82:
            return "Kralovehradecky kraj";
        case 83:
            return "Liberecky kraj";
        case 84:
            return "Olomoucky kraj";
        case 85:
            return "Moravskoslezsky kraj";
        case 86:
            return "Pardubicky kraj";
        case 87:
            return "Plzensky kraj";
        case 88:
            return "Stredocesky kraj";
        case 89:
            return "Ustecky kraj";
        case 90:
            return "Zlinsky kraj";
        }
    }else if (strcmp(country_code, "DE") == 0) {
        switch (region_code2) {
        case 1:
            return "Baden-Wurttemberg";
        case 2:
            return "Bayern";
        case 3:
            return "Bremen";
        case 4:
            return "Hamburg";
        case 5:
            return "Hessen";
        case 6:
            return "Niedersachsen";
        case 7:
            return "Nordrhein-Westfalen";
        case 8:
            return "Rheinland-Pfalz";
        case 9:
            return "Saarland";
        case 10:
            return "Schleswig-Holstein";
        case 11:
            return "Brandenburg";
        case 12:
            return "Mecklenburg-Vorpommern";
        case 13:
            return "Sachsen";
        case 14:
            return "Sachsen-Anhalt";
        case 15:
            return "Thuringen";
        case 16:
            return "Berlin";
        }
    }else if (strcmp(country_code, "DJ") == 0) {
        switch (region_code2) {
        case 1:
            return "Ali Sabieh";
        case 4:
            return "Obock";
        case 5:
            return "Tadjoura";
        case 6:
            return "Dikhil";
        case 7:
            return "Djibouti";
        case 8:
            return "Arta";
        }
    }else if (strcmp(country_code, "DK") == 0) {
        switch (region_code2) {
        case 17:
            return "Hovedstaden";
        case 18:
            return "Midtjylland";
        case 19:
            return "Nordjylland";
        case 20:
            return "Sjelland";
        case 21:
            return "Syddanmark";
        }
    }else if (strcmp(country_code, "DM") == 0) {
        switch (region_code2) {
        case 2:
            return "Saint Andrew";
        case 3:
            return "Saint David";
        case 4:
            return "Saint George";
        case 5:
            return "Saint John";
        case 6:
            return "Saint Joseph";
        case 7:
            return "Saint Luke";
        case 8:
            return "Saint Mark";
        case 9:
            return "Saint Patrick";
        case 10:
            return "Saint Paul";
        case 11:
            return "Saint Peter";
        }
    }else if (strcmp(country_code, "DO") == 0) {
        switch (region_code2) {
        case 1:
            return "Azua";
        case 2:
            return "Baoruco";
        case 3:
            return "Barahona";
        case 4:
            return "Dajabon";
        case 5:
            return "Distrito Nacional";
        case 6:
            return "Duarte";
        case 8:
            return "Espaillat";
        case 9:
            return "Independencia";
        case 10:
            return "La Altagracia";
        case 11:
            return "Elias Pina";
        case 12:
            return "La Romana";
        case 14:
            return "Maria Trinidad Sanchez";
        case 15:
            return "Monte Cristi";
        case 16:
            return "Pedernales";
        case 17:
            return "Peravia";
        case 18:
            return "Puerto Plata";
        case 19:
            return "Salcedo";
        case 20:
            return "Samana";
        case 21:
            return "Sanchez Ramirez";
        case 23:
            return "San Juan";
        case 24:
            return "San Pedro De Macoris";
        case 25:
            return "Santiago";
        case 26:
            return "Santiago Rodriguez";
        case 27:
            return "Valverde";
        case 28:
            return "El Seibo";
        case 29:
            return "Hato Mayor";
        case 30:
            return "La Vega";
        case 31:
            return "Monsenor Nouel";
        case 32:
            return "Monte Plata";
        case 33:
            return "San Cristobal";
        case 34:
            return "Distrito Nacional";
        case 35:
            return "Peravia";
        case 36:
            return "San Jose de Ocoa";
        case 37:
            return "Santo Domingo";
        }
    }else if (strcmp(country_code, "DZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Alger";
        case 3:
            return "Batna";
        case 4:
            return "Constantine";
        case 6:
            return "Medea";
        case 7:
            return "Mostaganem";
        case 9:
            return "Oran";
        case 10:
            return "Saida";
        case 12:
            return "Setif";
        case 13:
            return "Tiaret";
        case 14:
            return "Tizi Ouzou";
        case 15:
            return "Tlemcen";
        case 18:
            return "Bejaia";
        case 19:
            return "Biskra";
        case 20:
            return "Blida";
        case 21:
            return "Bouira";
        case 22:
            return "Djelfa";
        case 23:
            return "Guelma";
        case 24:
            return "Jijel";
        case 25:
            return "Laghouat";
        case 26:
            return "Mascara";
        case 27:
            return "M'sila";
        case 29:
            return "Oum el Bouaghi";
        case 30:
            return "Sidi Bel Abbes";
        case 31:
            return "Skikda";
        case 33:
            return "Tebessa";
        case 34:
            return "Adrar";
        case 35:
            return "Ain Defla";
        case 36:
            return "Ain Temouchent";
        case 37:
            return "Annaba";
        case 38:
            return "Bechar";
        case 39:
            return "Bordj Bou Arreridj";
        case 40:
            return "Boumerdes";
        case 41:
            return "Chlef";
        case 42:
            return "El Bayadh";
        case 43:
            return "El Oued";
        case 44:
            return "El Tarf";
        case 45:
            return "Ghardaia";
        case 46:
            return "Illizi";
        case 47:
            return "Khenchela";
        case 48:
            return "Mila";
        case 49:
            return "Naama";
        case 50:
            return "Ouargla";
        case 51:
            return "Relizane";
        case 52:
            return "Souk Ahras";
        case 53:
            return "Tamanghasset";
        case 54:
            return "Tindouf";
        case 55:
            return "Tipaza";
        case 56:
            return "Tissemsilt";
        }
    }else if (strcmp(country_code, "EC") == 0) {
        switch (region_code2) {
        case 1:
            return "Galapagos";
        case 2:
            return "Azuay";
        case 3:
            return "Bolivar";
        case 4:
            return "Canar";
        case 5:
            return "Carchi";
        case 6:
            return "Chimborazo";
        case 7:
            return "Cotopaxi";
        case 8:
            return "El Oro";
        case 9:
            return "Esmeraldas";
        case 10:
            return "Guayas";
        case 11:
            return "Imbabura";
        case 12:
            return "Loja";
        case 13:
            return "Los Rios";
        case 14:
            return "Manabi";
        case 15:
            return "Morona-Santiago";
        case 17:
            return "Pastaza";
        case 18:
            return "Pichincha";
        case 19:
            return "Tungurahua";
        case 20:
            return "Zamora-Chinchipe";
        case 22:
            return "Sucumbios";
        case 23:
            return "Napo";
        case 24:
            return "Orellana";
        }
    }else if (strcmp(country_code, "EE") == 0) {
        switch (region_code2) {
        case 1:
            return "Harjumaa";
        case 2:
            return "Hiiumaa";
        case 3:
            return "Ida-Virumaa";
        case 4:
            return "Jarvamaa";
        case 5:
            return "Jogevamaa";
        case 6:
            return "Kohtla-Jarve";
        case 7:
            return "Laanemaa";
        case 8:
            return "Laane-Virumaa";
        case 9:
            return "Narva";
        case 10:
            return "Parnu";
        case 11:
            return "Parnumaa";
        case 12:
            return "Polvamaa";
        case 13:
            return "Raplamaa";
        case 14:
            return "Saaremaa";
        case 15:
            return "Sillamae";
        case 16:
            return "Tallinn";
        case 17:
            return "Tartu";
        case 18:
            return "Tartumaa";
        case 19:
            return "Valgamaa";
        case 20:
            return "Viljandimaa";
        case 21:
            return "Vorumaa";
        }
    }else if (strcmp(country_code, "EG") == 0) {
        switch (region_code2) {
        case 1:
            return "Ad Daqahliyah";
        case 2:
            return "Al Bahr al Ahmar";
        case 3:
            return "Al Buhayrah";
        case 4:
            return "Al Fayyum";
        case 5:
            return "Al Gharbiyah";
        case 6:
            return "Al Iskandariyah";
        case 7:
            return "Al Isma'iliyah";
        case 8:
            return "Al Jizah";
        case 9:
            return "Al Minufiyah";
        case 10:
            return "Al Minya";
        case 11:
            return "Al Qahirah";
        case 12:
            return "Al Qalyubiyah";
        case 13:
            return "Al Wadi al Jadid";
        case 14:
            return "Ash Sharqiyah";
        case 15:
            return "As Suways";
        case 16:
            return "Aswan";
        case 17:
            return "Asyut";
        case 18:
            return "Bani Suwayf";
        case 19:
            return "Bur Sa'id";
        case 20:
            return "Dumyat";
        case 21:
            return "Kafr ash Shaykh";
        case 22:
            return "Matruh";
        case 23:
            return "Qina";
        case 24:
            return "Suhaj";
        case 26:
            return "Janub Sina'";
        case 27:
            return "Shamal Sina'";
        case 28:
            return "Al Uqsur";
        }
    }else if (strcmp(country_code, "ER") == 0) {
        switch (region_code2) {
        case 1:
            return "Anseba";
        case 2:
            return "Debub";
        case 3:
            return "Debubawi K'eyih Bahri";
        case 4:
            return "Gash Barka";
        case 5:
            return "Ma'akel";
        case 6:
            return "Semenawi K'eyih Bahri";
        }
    }else if (strcmp(country_code, "ES") == 0) {
        switch (region_code2) {
        case 7:
            return "Islas Baleares";
        case 27:
            return "La Rioja";
        case 29:
            return "Madrid";
        case 31:
            return "Murcia";
        case 32:
            return "Navarra";
        case 34:
            return "Asturias";
        case 39:
            return "Cantabria";
        case 51:
            return "Andalucia";
        case 52:
            return "Aragon";
        case 53:
            return "Canarias";
        case 54:
            return "Castilla-La Mancha";
        case 55:
            return "Castilla y Leon";
        case 56:
            return "Catalonia";
        case 57:
            return "Extremadura";
        case 58:
            return "Galicia";
        case 59:
            return "Pais Vasco";
        case 60:
            return "Comunidad Valenciana";
        }
    }else if (strcmp(country_code, "ET") == 0) {
        switch (region_code2) {
        case 44:
            return "Adis Abeba";
        case 45:
            return "Afar";
        case 46:
            return "Amara";
        case 47:
            return "Binshangul Gumuz";
        case 48:
            return "Dire Dawa";
        case 49:
            return "Gambela Hizboch";
        case 50:
            return "Hareri Hizb";
        case 51:
            return "Oromiya";
        case 52:
            return "Sumale";
        case 53:
            return "Tigray";
        case 54:
            return "YeDebub Biheroch Bihereseboch na Hizboch";
        }
    }else if (strcmp(country_code, "FI") == 0) {
        switch (region_code2) {
        case 1:
            return "Aland";
        case 6:
            return "Lapland";
        case 8:
            return "Oulu";
        case 13:
            return "Southern Finland";
        case 14:
            return "Eastern Finland";
        case 15:
            return "Western Finland";
        }
    }else if (strcmp(country_code, "FJ") == 0) {
        switch (region_code2) {
        case 1:
            return "Central";
        case 2:
            return "Eastern";
        case 3:
            return "Northern";
        case 4:
            return "Rotuma";
        case 5:
            return "Western";
        }
    }else if (strcmp(country_code, "FM") == 0) {
        switch (region_code2) {
        case 1:
            return "Kosrae";
        case 2:
            return "Pohnpei";
        case 3:
            return "Chuuk";
        case 4:
            return "Yap";
        }
    }else if (strcmp(country_code, "FR") == 0) {
        switch (region_code2) {
        case 97:
            return "Aquitaine";
        case 98:
            return "Auvergne";
        case 99:
            return "Basse-Normandie";
        case 832:
            return "Bourgogne";
        case 833:
            return "Bretagne";
        case 834:
            return "Centre";
        case 835:
            return "Champagne-Ardenne";
        case 836:
            return "Corse";
        case 837:
            return "Franche-Comte";
        case 838:
            return "Haute-Normandie";
        case 839:
            return "Ile-de-France";
        case 840:
            return "Languedoc-Roussillon";
        case 875:
            return "Limousin";
        case 876:
            return "Lorraine";
        case 877:
            return "Midi-Pyrenees";
        case 878:
            return "Nord-Pas-de-Calais";
        case 879:
            return "Pays de la Loire";
        case 880:
            return "Picardie";
        case 881:
            return "Poitou-Charentes";
        case 882:
            return "Provence-Alpes-Cote d'Azur";
        case 883:
            return "Rhone-Alpes";
        case 918:
            return "Alsace";
        }
    }else if (strcmp(country_code, "GA") == 0) {
        switch (region_code2) {
        case 1:
            return "Estuaire";
        case 2:
            return "Haut-Ogooue";
        case 3:
            return "Moyen-Ogooue";
        case 4:
            return "Ngounie";
        case 5:
            return "Nyanga";
        case 6:
            return "Ogooue-Ivindo";
        case 7:
            return "Ogooue-Lolo";
        case 8:
            return "Ogooue-Maritime";
        case 9:
            return "Woleu-Ntem";
        }
    }else if (strcmp(country_code, "GB") == 0) {
        switch (region_code2) {
        case 832:
            return "Barking and Dagenham";
        case 833:
            return "Barnet";
        case 834:
            return "Barnsley";
        case 835:
            return "Bath and North East Somerset";
        case 836:
            return "Bedfordshire";
        case 837:
            return "Bexley";
        case 838:
            return "Birmingham";
        case 839:
            return "Blackburn with Darwen";
        case 840:
            return "Blackpool";
        case 875:
            return "Bolton";
        case 876:
            return "Bournemouth";
        case 877:
            return "Bracknell Forest";
        case 878:
            return "Bradford";
        case 879:
            return "Brent";
        case 880:
            return "Brighton and Hove";
        case 881:
            return "Bristol, City of";
        case 882:
            return "Bromley";
        case 883:
            return "Buckinghamshire";
        case 918:
            return "Bury";
        case 919:
            return "Calderdale";
        case 920:
            return "Cambridgeshire";
        case 921:
            return "Camden";
        case 922:
            return "Cheshire";
        case 923:
            return "Cornwall";
        case 924:
            return "Coventry";
        case 925:
            return "Croydon";
        case 926:
            return "Cumbria";
        case 961:
            return "Darlington";
        case 962:
            return "Derby";
        case 963:
            return "Derbyshire";
        case 964:
            return "Devon";
        case 965:
            return "Doncaster";
        case 966:
            return "Dorset";
        case 967:
            return "Dudley";
        case 968:
            return "Durham";
        case 969:
            return "Ealing";
        case 1004:
            return "East Riding of Yorkshire";
        case 1005:
            return "East Sussex";
        case 1006:
            return "Enfield";
        case 1007:
            return "Essex";
        case 1008:
            return "Gateshead";
        case 1009:
            return "Gloucestershire";
        case 1010:
            return "Greenwich";
        case 1011:
            return "Hackney";
        case 1012:
            return "Halton";
        case 1047:
            return "Hammersmith and Fulham";
        case 1048:
            return "Hampshire";
        case 1049:
            return "Haringey";
        case 1050:
            return "Harrow";
        case 1051:
            return "Hartlepool";
        case 1052:
            return "Havering";
        case 1053:
            return "Herefordshire";
        case 1054:
            return "Hertford";
        case 1055:
            return "Hillingdon";
        case 1090:
            return "Hounslow";
        case 1091:
            return "Isle of Wight";
        case 1092:
            return "Islington";
        case 1093:
            return "Kensington and Chelsea";
        case 1094:
            return "Kent";
        case 1095:
            return "Kingston upon Hull, City of";
        case 1096:
            return "Kingston upon Thames";
        case 1097:
            return "Kirklees";
        case 1098:
            return "Knowsley";
        case 1133:
            return "Lambeth";
        case 1134:
            return "Lancashire";
        case 1135:
            return "Leeds";
        case 1136:
            return "Leicester";
        case 1137:
            return "Leicestershire";
        case 1138:
            return "Lewisham";
        case 1139:
            return "Lincolnshire";
        case 1140:
            return "Liverpool";
        case 1141:
            return "London, City of";
        case 1176:
            return "Luton";
        case 1177:
            return "Manchester";
        case 1178:
            return "Medway";
        case 1179:
            return "Merton";
        case 1180:
            return "Middlesbrough";
        case 1181:
            return "Milton Keynes";
        case 1182:
            return "Newcastle upon Tyne";
        case 1183:
            return "Newham";
        case 1184:
            return "Norfolk";
        case 1219:
            return "Northamptonshire";
        case 1220:
            return "North East Lincolnshire";
        case 1221:
            return "North Lincolnshire";
        case 1222:
            return "North Somerset";
        case 1223:
            return "North Tyneside";
        case 1224:
            return "Northumberland";
        case 1225:
            return "North Yorkshire";
        case 1226:
            return "Nottingham";
        case 1227:
            return "Nottinghamshire";
        case 1262:
            return "Oldham";
        case 1263:
            return "Oxfordshire";
        case 1264:
            return "Peterborough";
        case 1265:
            return "Plymouth";
        case 1266:
            return "Poole";
        case 1267:
            return "Portsmouth";
        case 1268:
            return "Reading";
        case 1269:
            return "Redbridge";
        case 1270:
            return "Redcar and Cleveland";
        case 1305:
            return "Richmond upon Thames";
        case 1306:
            return "Rochdale";
        case 1307:
            return "Rotherham";
        case 1308:
            return "Rutland";
        case 1309:
            return "Salford";
        case 1310:
            return "Shropshire";
        case 1311:
            return "Sandwell";
        case 1312:
            return "Sefton";
        case 1313:
            return "Sheffield";
        case 1348:
            return "Slough";
        case 1349:
            return "Solihull";
        case 1350:
            return "Somerset";
        case 1351:
            return "Southampton";
        case 1352:
            return "Southend-on-Sea";
        case 1353:
            return "South Gloucestershire";
        case 1354:
            return "South Tyneside";
        case 1355:
            return "Southwark";
        case 1356:
            return "Staffordshire";
        case 1391:
            return "St. Helens";
        case 1392:
            return "Stockport";
        case 1393:
            return "Stockton-on-Tees";
        case 1394:
            return "Stoke-on-Trent";
        case 1395:
            return "Suffolk";
        case 1396:
            return "Sunderland";
        case 1397:
            return "Surrey";
        case 1398:
            return "Sutton";
        case 1399:
            return "Swindon";
        case 1434:
            return "Tameside";
        case 1435:
            return "Telford and Wrekin";
        case 1436:
            return "Thurrock";
        case 1437:
            return "Torbay";
        case 1438:
            return "Tower Hamlets";
        case 1439:
            return "Trafford";
        case 1440:
            return "Wakefield";
        case 1441:
            return "Walsall";
        case 1442:
            return "Waltham Forest";
        case 1477:
            return "Wandsworth";
        case 1478:
            return "Warrington";
        case 1479:
            return "Warwickshire";
        case 1480:
            return "West Berkshire";
        case 1481:
            return "Westminster";
        case 1482:
            return "West Sussex";
        case 1483:
            return "Wigan";
        case 1484:
            return "Wiltshire";
        case 1485:
            return "Windsor and Maidenhead";
        case 1520:
            return "Wirral";
        case 1521:
            return "Wokingham";
        case 1522:
            return "Wolverhampton";
        case 1523:
            return "Worcestershire";
        case 1524:
            return "York";
        case 1525:
            return "Antrim";
        case 1526:
            return "Ards";
        case 1527:
            return "Armagh";
        case 1528:
            return "Ballymena";
        case 1563:
            return "Ballymoney";
        case 1564:
            return "Banbridge";
        case 1565:
            return "Belfast";
        case 1566:
            return "Carrickfergus";
        case 1567:
            return "Castlereagh";
        case 1568:
            return "Coleraine";
        case 1569:
            return "Cookstown";
        case 1570:
            return "Craigavon";
        case 1571:
            return "Down";
        case 1606:
            return "Dungannon";
        case 1607:
            return "Fermanagh";
        case 1608:
            return "Larne";
        case 1609:
            return "Limavady";
        case 1610:
            return "Lisburn";
        case 1611:
            return "Derry";
        case 1612:
            return "Magherafelt";
        case 1613:
            return "Moyle";
        case 1614:
            return "Newry and Mourne";
        case 1649:
            return "Newtownabbey";
        case 1650:
            return "North Down";
        case 1651:
            return "Omagh";
        case 1652:
            return "Strabane";
        case 1653:
            return "Aberdeen City";
        case 1654:
            return "Aberdeenshire";
        case 1655:
            return "Angus";
        case 1656:
            return "Argyll and Bute";
        case 1657:
            return "Scottish Borders, The";
        case 1692:
            return "Clackmannanshire";
        case 1693:
            return "Dumfries and Galloway";
        case 1694:
            return "Dundee City";
        case 1695:
            return "East Ayrshire";
        case 1696:
            return "East Dunbartonshire";
        case 1697:
            return "East Lothian";
        case 1698:
            return "East Renfrewshire";
        case 1699:
            return "Edinburgh, City of";
        case 1700:
            return "Falkirk";
        case 1735:
            return "Fife";
        case 1736:
            return "Glasgow City";
        case 1737:
            return "Highland";
        case 1738:
            return "Inverclyde";
        case 1739:
            return "Midlothian";
        case 1740:
            return "Moray";
        case 1741:
            return "North Ayrshire";
        case 1742:
            return "North Lanarkshire";
        case 1743:
            return "Orkney";
        case 1778:
            return "Perth and Kinross";
        case 1779:
            return "Renfrewshire";
        case 1780:
            return "Shetland Islands";
        case 1781:
            return "South Ayrshire";
        case 1782:
            return "South Lanarkshire";
        case 1783:
            return "Stirling";
        case 1784:
            return "West Dunbartonshire";
        case 1785:
            return "Eilean Siar";
        case 1786:
            return "West Lothian";
        case 1821:
            return "Isle of Anglesey";
        case 1822:
            return "Blaenau Gwent";
        case 1823:
            return "Bridgend";
        case 1824:
            return "Caerphilly";
        case 1825:
            return "Cardiff";
        case 1826:
            return "Ceredigion";
        case 1827:
            return "Carmarthenshire";
        case 1828:
            return "Conwy";
        case 1829:
            return "Denbighshire";
        case 1864:
            return "Flintshire";
        case 1865:
            return "Gwynedd";
        case 1866:
            return "Merthyr Tydfil";
        case 1867:
            return "Monmouthshire";
        case 1868:
            return "Neath Port Talbot";
        case 1869:
            return "Newport";
        case 1870:
            return "Pembrokeshire";
        case 1871:
            return "Powys";
        case 1872:
            return "Rhondda Cynon Taff";
        case 1907:
            return "Swansea";
        case 1908:
            return "Torfaen";
        case 1909:
            return "Vale of Glamorgan, The";
        case 1910:
            return "Wrexham";
        case 1911:
            return "Bedfordshire";
        case 1912:
            return "Central Bedfordshire";
        case 1913:
            return "Cheshire East";
        case 1914:
            return "Cheshire West and Chester";
        case 1915:
            return "Isles of Scilly";
        }
    }else if (strcmp(country_code, "GD") == 0) {
        switch (region_code2) {
        case 1:
            return "Saint Andrew";
        case 2:
            return "Saint David";
        case 3:
            return "Saint George";
        case 4:
            return "Saint John";
        case 5:
            return "Saint Mark";
        case 6:
            return "Saint Patrick";
        }
    }else if (strcmp(country_code, "GE") == 0) {
        switch (region_code2) {
        case 1:
            return "Abashis Raioni";
        case 2:
            return "Abkhazia";
        case 3:
            return "Adigenis Raioni";
        case 4:
            return "Ajaria";
        case 5:
            return "Akhalgoris Raioni";
        case 6:
            return "Akhalk'alak'is Raioni";
        case 7:
            return "Akhalts'ikhis Raioni";
        case 8:
            return "Akhmetis Raioni";
        case 9:
            return "Ambrolauris Raioni";
        case 10:
            return "Aspindzis Raioni";
        case 11:
            return "Baghdat'is Raioni";
        case 12:
            return "Bolnisis Raioni";
        case 13:
            return "Borjomis Raioni";
        case 14:
            return "Chiat'ura";
        case 15:
            return "Ch'khorotsqus Raioni";
        case 16:
            return "Ch'okhatauris Raioni";
        case 17:
            return "Dedop'listsqaros Raioni";
        case 18:
            return "Dmanisis Raioni";
        case 19:
            return "Dushet'is Raioni";
        case 20:
            return "Gardabanis Raioni";
        case 21:
            return "Gori";
        case 22:
            return "Goris Raioni";
        case 23:
            return "Gurjaanis Raioni";
        case 24:
            return "Javis Raioni";
        case 25:
            return "K'arelis Raioni";
        case 26:
            return "Kaspis Raioni";
        case 27:
            return "Kharagaulis Raioni";
        case 28:
            return "Khashuris Raioni";
        case 29:
            return "Khobis Raioni";
        case 30:
            return "Khonis Raioni";
        case 31:
            return "K'ut'aisi";
        case 32:
            return "Lagodekhis Raioni";
        case 33:
            return "Lanch'khut'is Raioni";
        case 34:
            return "Lentekhis Raioni";
        case 35:
            return "Marneulis Raioni";
        case 36:
            return "Martvilis Raioni";
        case 37:
            return "Mestiis Raioni";
        case 38:
            return "Mts'khet'is Raioni";
        case 39:
            return "Ninotsmindis Raioni";
        case 40:
            return "Onis Raioni";
        case 41:
            return "Ozurget'is Raioni";
        case 42:
            return "P'ot'i";
        case 43:
            return "Qazbegis Raioni";
        case 44:
            return "Qvarlis Raioni";
        case 45:
            return "Rust'avi";
        case 46:
            return "Sach'kheris Raioni";
        case 47:
            return "Sagarejos Raioni";
        case 48:
            return "Samtrediis Raioni";
        case 49:
            return "Senakis Raioni";
        case 50:
            return "Sighnaghis Raioni";
        case 51:
            return "T'bilisi";
        case 52:
            return "T'elavis Raioni";
        case 53:
            return "T'erjolis Raioni";
        case 54:
            return "T'et'ritsqaros Raioni";
        case 55:
            return "T'ianet'is Raioni";
        case 56:
            return "Tqibuli";
        case 57:
            return "Ts'ageris Raioni";
        case 58:
            return "Tsalenjikhis Raioni";
        case 59:
            return "Tsalkis Raioni";
        case 60:
            return "Tsqaltubo";
        case 61:
            return "Vanis Raioni";
        case 62:
            return "Zestap'onis Raioni";
        case 63:
            return "Zugdidi";
        case 64:
            return "Zugdidis Raioni";
        }
    }else if (strcmp(country_code, "GH") == 0) {
        switch (region_code2) {
        case 1:
            return "Greater Accra";
        case 2:
            return "Ashanti";
        case 3:
            return "Brong-Ahafo";
        case 4:
            return "Central";
        case 5:
            return "Eastern";
        case 6:
            return "Northern";
        case 8:
            return "Volta";
        case 9:
            return "Western";
        case 10:
            return "Upper East";
        case 11:
            return "Upper West";
        }
    }else if (strcmp(country_code, "GL") == 0) {
        switch (region_code2) {
        case 1:
            return "Nordgronland";
        case 2:
            return "Ostgronland";
        case 3:
            return "Vestgronland";
        }
    }else if (strcmp(country_code, "GM") == 0) {
        switch (region_code2) {
        case 1:
            return "Banjul";
        case 2:
            return "Lower River";
        case 3:
            return "Central River";
        case 4:
            return "Upper River";
        case 5:
            return "Western";
        case 7:
            return "North Bank";
        }
    }else if (strcmp(country_code, "GN") == 0) {
        switch (region_code2) {
        case 1:
            return "Beyla";
        case 2:
            return "Boffa";
        case 3:
            return "Boke";
        case 4:
            return "Conakry";
        case 5:
            return "Dabola";
        case 6:
            return "Dalaba";
        case 7:
            return "Dinguiraye";
        case 9:
            return "Faranah";
        case 10:
            return "Forecariah";
        case 11:
            return "Fria";
        case 12:
            return "Gaoual";
        case 13:
            return "Gueckedou";
        case 15:
            return "Kerouane";
        case 16:
            return "Kindia";
        case 17:
            return "Kissidougou";
        case 18:
            return "Koundara";
        case 19:
            return "Kouroussa";
        case 21:
            return "Macenta";
        case 22:
            return "Mali";
        case 23:
            return "Mamou";
        case 25:
            return "Pita";
        case 27:
            return "Telimele";
        case 28:
            return "Tougue";
        case 29:
            return "Yomou";
        case 30:
            return "Coyah";
        case 31:
            return "Dubreka";
        case 32:
            return "Kankan";
        case 33:
            return "Koubia";
        case 34:
            return "Labe";
        case 35:
            return "Lelouma";
        case 36:
            return "Lola";
        case 37:
            return "Mandiana";
        case 38:
            return "Nzerekore";
        case 39:
            return "Siguiri";
        }
    }else if (strcmp(country_code, "GQ") == 0) {
        switch (region_code2) {
        case 3:
            return "Annobon";
        case 4:
            return "Bioko Norte";
        case 5:
            return "Bioko Sur";
        case 6:
            return "Centro Sur";
        case 7:
            return "Kie-Ntem";
        case 8:
            return "Litoral";
        case 9:
            return "Wele-Nzas";
        }
    }else if (strcmp(country_code, "GR") == 0) {
        switch (region_code2) {
        case 1:
            return "Evros";
        case 2:
            return "Rodhopi";
        case 3:
            return "Xanthi";
        case 4:
            return "Drama";
        case 5:
            return "Serrai";
        case 6:
            return "Kilkis";
        case 7:
            return "Pella";
        case 8:
            return "Florina";
        case 9:
            return "Kastoria";
        case 10:
            return "Grevena";
        case 11:
            return "Kozani";
        case 12:
            return "Imathia";
        case 13:
            return "Thessaloniki";
        case 14:
            return "Kavala";
        case 15:
            return "Khalkidhiki";
        case 16:
            return "Pieria";
        case 17:
            return "Ioannina";
        case 18:
            return "Thesprotia";
        case 19:
            return "Preveza";
        case 20:
            return "Arta";
        case 21:
            return "Larisa";
        case 22:
            return "Trikala";
        case 23:
            return "Kardhitsa";
        case 24:
            return "Magnisia";
        case 25:
            return "Kerkira";
        case 26:
            return "Levkas";
        case 27:
            return "Kefallinia";
        case 28:
            return "Zakinthos";
        case 29:
            return "Fthiotis";
        case 30:
            return "Evritania";
        case 31:
            return "Aitolia kai Akarnania";
        case 32:
            return "Fokis";
        case 33:
            return "Voiotia";
        case 34:
            return "Evvoia";
        case 35:
            return "Attiki";
        case 36:
            return "Argolis";
        case 37:
            return "Korinthia";
        case 38:
            return "Akhaia";
        case 39:
            return "Ilia";
        case 40:
            return "Messinia";
        case 41:
            return "Arkadhia";
        case 42:
            return "Lakonia";
        case 43:
            return "Khania";
        case 44:
            return "Rethimni";
        case 45:
            return "Iraklion";
        case 46:
            return "Lasithi";
        case 47:
            return "Dhodhekanisos";
        case 48:
            return "Samos";
        case 49:
            return "Kikladhes";
        case 50:
            return "Khios";
        case 51:
            return "Lesvos";
        }
    }else if (strcmp(country_code, "GT") == 0) {
        switch (region_code2) {
        case 1:
            return "Alta Verapaz";
        case 2:
            return "Baja Verapaz";
        case 3:
            return "Chimaltenango";
        case 4:
            return "Chiquimula";
        case 5:
            return "El Progreso";
        case 6:
            return "Escuintla";
        case 7:
            return "Guatemala";
        case 8:
            return "Huehuetenango";
        case 9:
            return "Izabal";
        case 10:
            return "Jalapa";
        case 11:
            return "Jutiapa";
        case 12:
            return "Peten";
        case 13:
            return "Quetzaltenango";
        case 14:
            return "Quiche";
        case 15:
            return "Retalhuleu";
        case 16:
            return "Sacatepequez";
        case 17:
            return "San Marcos";
        case 18:
            return "Santa Rosa";
        case 19:
            return "Solola";
        case 20:
            return "Suchitepequez";
        case 21:
            return "Totonicapan";
        case 22:
            return "Zacapa";
        }
    }else if (strcmp(country_code, "GW") == 0) {
        switch (region_code2) {
        case 1:
            return "Bafata";
        case 2:
            return "Quinara";
        case 4:
            return "Oio";
        case 5:
            return "Bolama";
        case 6:
            return "Cacheu";
        case 7:
            return "Tombali";
        case 10:
            return "Gabu";
        case 11:
            return "Bissau";
        case 12:
            return "Biombo";
        }
    }else if (strcmp(country_code, "GY") == 0) {
        switch (region_code2) {
        case 10:
            return "Barima-Waini";
        case 11:
            return "Cuyuni-Mazaruni";
        case 12:
            return "Demerara-Mahaica";
        case 13:
            return "East Berbice-Corentyne";
        case 14:
            return "Essequibo Islands-West Demerara";
        case 15:
            return "Mahaica-Berbice";
        case 16:
            return "Pomeroon-Supenaam";
        case 17:
            return "Potaro-Siparuni";
        case 18:
            return "Upper Demerara-Berbice";
        case 19:
            return "Upper Takutu-Upper Essequibo";
        }
    }else if (strcmp(country_code, "HN") == 0) {
        switch (region_code2) {
        case 1:
            return "Atlantida";
        case 2:
            return "Choluteca";
        case 3:
            return "Colon";
        case 4:
            return "Comayagua";
        case 5:
            return "Copan";
        case 6:
            return "Cortes";
        case 7:
            return "El Paraiso";
        case 8:
            return "Francisco Morazan";
        case 9:
            return "Gracias a Dios";
        case 10:
            return "Intibuca";
        case 11:
            return "Islas de la Bahia";
        case 12:
            return "La Paz";
        case 13:
            return "Lempira";
        case 14:
            return "Ocotepeque";
        case 15:
            return "Olancho";
        case 16:
            return "Santa Barbara";
        case 17:
            return "Valle";
        case 18:
            return "Yoro";
        }
    }else if (strcmp(country_code, "HR") == 0) {
        switch (region_code2) {
        case 1:
            return "Bjelovarsko-Bilogorska";
        case 2:
            return "Brodsko-Posavska";
        case 3:
            return "Dubrovacko-Neretvanska";
        case 4:
            return "Istarska";
        case 5:
            return "Karlovacka";
        case 6:
            return "Koprivnicko-Krizevacka";
        case 7:
            return "Krapinsko-Zagorska";
        case 8:
            return "Licko-Senjska";
        case 9:
            return "Medimurska";
        case 10:
            return "Osjecko-Baranjska";
        case 11:
            return "Pozesko-Slavonska";
        case 12:
            return "Primorsko-Goranska";
        case 13:
            return "Sibensko-Kninska";
        case 14:
            return "Sisacko-Moslavacka";
        case 15:
            return "Splitsko-Dalmatinska";
        case 16:
            return "Varazdinska";
        case 17:
            return "Viroviticko-Podravska";
        case 18:
            return "Vukovarsko-Srijemska";
        case 19:
            return "Zadarska";
        case 20:
            return "Zagrebacka";
        case 21:
            return "Grad Zagreb";
        }
    }else if (strcmp(country_code, "HT") == 0) {
        switch (region_code2) {
        case 3:
            return "Nord-Ouest";
        case 6:
            return "Artibonite";
        case 7:
            return "Centre";
        case 9:
            return "Nord";
        case 10:
            return "Nord-Est";
        case 11:
            return "Ouest";
        case 12:
            return "Sud";
        case 13:
            return "Sud-Est";
        case 14:
            return "Grand' Anse";
        case 15:
            return "Nippes";
        }
    }else if (strcmp(country_code, "HU") == 0) {
        switch (region_code2) {
        case 1:
            return "Bacs-Kiskun";
        case 2:
            return "Baranya";
        case 3:
            return "Bekes";
        case 4:
            return "Borsod-Abauj-Zemplen";
        case 5:
            return "Budapest";
        case 6:
            return "Csongrad";
        case 7:
            return "Debrecen";
        case 8:
            return "Fejer";
        case 9:
            return "Gyor-Moson-Sopron";
        case 10:
            return "Hajdu-Bihar";
        case 11:
            return "Heves";
        case 12:
            return "Komarom-Esztergom";
        case 13:
            return "Miskolc";
        case 14:
            return "Nograd";
        case 15:
            return "Pecs";
        case 16:
            return "Pest";
        case 17:
            return "Somogy";
        case 18:
            return "Szabolcs-Szatmar-Bereg";
        case 19:
            return "Szeged";
        case 20:
            return "Jasz-Nagykun-Szolnok";
        case 21:
            return "Tolna";
        case 22:
            return "Vas";
        case 23:
            return "Veszprem";
        case 24:
            return "Zala";
        case 25:
            return "Gyor";
        case 26:
            return "Bekescsaba";
        case 27:
            return "Dunaujvaros";
        case 28:
            return "Eger";
        case 29:
            return "Hodmezovasarhely";
        case 30:
            return "Kaposvar";
        case 31:
            return "Kecskemet";
        case 32:
            return "Nagykanizsa";
        case 33:
            return "Nyiregyhaza";
        case 34:
            return "Sopron";
        case 35:
            return "Szekesfehervar";
        case 36:
            return "Szolnok";
        case 37:
            return "Szombathely";
        case 38:
            return "Tatabanya";
        case 39:
            return "Veszprem";
        case 40:
            return "Zalaegerszeg";
        case 41:
            return "Salgotarjan";
        case 42:
            return "Szekszard";
        case 43:
            return "Erd";
        }
    }else if (strcmp(country_code, "ID") == 0) {
        switch (region_code2) {
        case 1:
            return "Aceh";
        case 2:
            return "Bali";
        case 3:
            return "Bengkulu";
        case 4:
            return "Jakarta Raya";
        case 5:
            return "Jambi";
        case 7:
            return "Jawa Tengah";
        case 8:
            return "Jawa Timur";
        case 10:
            return "Yogyakarta";
        case 11:
            return "Kalimantan Barat";
        case 12:
            return "Kalimantan Selatan";
        case 13:
            return "Kalimantan Tengah";
        case 14:
            return "Kalimantan Timur";
        case 15:
            return "Lampung";
        case 17:
            return "Nusa Tenggara Barat";
        case 18:
            return "Nusa Tenggara Timur";
        case 21:
            return "Sulawesi Tengah";
        case 22:
            return "Sulawesi Tenggara";
        case 24:
            return "Sumatera Barat";
        case 26:
            return "Sumatera Utara";
        case 28:
            return "Maluku";
        case 29:
            return "Maluku Utara";
        case 30:
            return "Jawa Barat";
        case 31:
            return "Sulawesi Utara";
        case 32:
            return "Sumatera Selatan";
        case 33:
            return "Banten";
        case 34:
            return "Gorontalo";
        case 35:
            return "Kepulauan Bangka Belitung";
        case 36:
            return "Papua";
        case 37:
            return "Riau";
        case 38:
            return "Sulawesi Selatan";
        case 39:
            return "Irian Jaya Barat";
        case 40:
            return "Kepulauan Riau";
        case 41:
            return "Sulawesi Barat";
        }
    }else if (strcmp(country_code, "IE") == 0) {
        switch (region_code2) {
        case 1:
            return "Carlow";
        case 2:
            return "Cavan";
        case 3:
            return "Clare";
        case 4:
            return "Cork";
        case 6:
            return "Donegal";
        case 7:
            return "Dublin";
        case 10:
            return "Galway";
        case 11:
            return "Kerry";
        case 12:
            return "Kildare";
        case 13:
            return "Kilkenny";
        case 14:
            return "Leitrim";
        case 15:
            return "Laois";
        case 16:
            return "Limerick";
        case 18:
            return "Longford";
        case 19:
            return "Louth";
        case 20:
            return "Mayo";
        case 21:
            return "Meath";
        case 22:
            return "Monaghan";
        case 23:
            return "Offaly";
        case 24:
            return "Roscommon";
        case 25:
            return "Sligo";
        case 26:
            return "Tipperary";
        case 27:
            return "Waterford";
        case 29:
            return "Westmeath";
        case 30:
            return "Wexford";
        case 31:
            return "Wicklow";
        }
    }else if (strcmp(country_code, "IL") == 0) {
        switch (region_code2) {
        case 1:
            return "HaDarom";
        case 2:
            return "HaMerkaz";
        case 3:
            return "HaZafon";
        case 4:
            return "Hefa";
        case 5:
            return "Tel Aviv";
        case 6:
            return "Yerushalayim";
        }
    }else if (strcmp(country_code, "IN") == 0) {
        switch (region_code2) {
        case 1:
            return "Andaman and Nicobar Islands";
        case 2:
            return "Andhra Pradesh";
        case 3:
            return "Assam";
        case 5:
            return "Chandigarh";
        case 6:
            return "Dadra and Nagar Haveli";
        case 7:
            return "Delhi";
        case 9:
            return "Gujarat";
        case 10:
            return "Haryana";
        case 11:
            return "Himachal Pradesh";
        case 12:
            return "Jammu and Kashmir";
        case 13:
            return "Kerala";
        case 14:
            return "Lakshadweep";
        case 16:
            return "Maharashtra";
        case 17:
            return "Manipur";
        case 18:
            return "Meghalaya";
        case 19:
            return "Karnataka";
        case 20:
            return "Nagaland";
        case 21:
            return "Orissa";
        case 22:
            return "Puducherry";
        case 23:
            return "Punjab";
        case 24:
            return "Rajasthan";
        case 25:
            return "Tamil Nadu";
        case 26:
            return "Tripura";
        case 28:
            return "West Bengal";
        case 29:
            return "Sikkim";
        case 30:
            return "Arunachal Pradesh";
        case 31:
            return "Mizoram";
        case 32:
            return "Daman and Diu";
        case 33:
            return "Goa";
        case 34:
            return "Bihar";
        case 35:
            return "Madhya Pradesh";
        case 36:
            return "Uttar Pradesh";
        case 37:
            return "Chhattisgarh";
        case 38:
            return "Jharkhand";
        case 39:
            return "Uttarakhand";
        }
    }else if (strcmp(country_code, "IQ") == 0) {
        switch (region_code2) {
        case 1:
            return "Al Anbar";
        case 2:
            return "Al Basrah";
        case 3:
            return "Al Muthanna";
        case 4:
            return "Al Qadisiyah";
        case 5:
            return "As Sulaymaniyah";
        case 6:
            return "Babil";
        case 7:
            return "Baghdad";
        case 8:
            return "Dahuk";
        case 9:
            return "Dhi Qar";
        case 10:
            return "Diyala";
        case 11:
            return "Arbil";
        case 12:
            return "Karbala'";
        case 13:
            return "At Ta'mim";
        case 14:
            return "Maysan";
        case 15:
            return "Ninawa";
        case 16:
            return "Wasit";
        case 17:
            return "An Najaf";
        case 18:
            return "Salah ad Din";
        }
    }else if (strcmp(country_code, "IR") == 0) {
        switch (region_code2) {
        case 1:
            return "Azarbayjan-e Bakhtari";
        case 3:
            return "Chahar Mahall va Bakhtiari";
        case 4:
            return "Sistan va Baluchestan";
        case 5:
            return "Kohkiluyeh va Buyer Ahmadi";
        case 7:
            return "Fars";
        case 8:
            return "Gilan";
        case 9:
            return "Hamadan";
        case 10:
            return "Ilam";
        case 11:
            return "Hormozgan";
        case 12:
            return "Kerman";
        case 13:
            return "Bakhtaran";
        case 15:
            return "Khuzestan";
        case 16:
            return "Kordestan";
        case 17:
            return "Mazandaran";
        case 18:
            return "Semnan Province";
        case 19:
            return "Markazi";
        case 21:
            return "Zanjan";
        case 22:
            return "Bushehr";
        case 23:
            return "Lorestan";
        case 24:
            return "Markazi";
        case 25:
            return "Semnan";
        case 26:
            return "Tehran";
        case 27:
            return "Zanjan";
        case 28:
            return "Esfahan";
        case 29:
            return "Kerman";
        case 30:
            return "Khorasan";
        case 31:
            return "Yazd";
        case 32:
            return "Ardabil";
        case 33:
            return "East Azarbaijan";
        case 34:
            return "Markazi";
        case 35:
            return "Mazandaran";
        case 36:
            return "Zanjan";
        case 37:
            return "Golestan";
        case 38:
            return "Qazvin";
        case 39:
            return "Qom";
        case 40:
            return "Yazd";
        case 41:
            return "Khorasan-e Janubi";
        case 42:
            return "Khorasan-e Razavi";
        case 43:
            return "Khorasan-e Shemali";
        case 44:
            return "Alborz";
        }
    }else if (strcmp(country_code, "IS") == 0) {
        switch (region_code2) {
        case 3:
            return "Arnessysla";
        case 5:
            return "Austur-Hunavatnssysla";
        case 6:
            return "Austur-Skaftafellssysla";
        case 7:
            return "Borgarfjardarsysla";
        case 9:
            return "Eyjafjardarsysla";
        case 10:
            return "Gullbringusysla";
        case 15:
            return "Kjosarsysla";
        case 17:
            return "Myrasysla";
        case 20:
            return "Nordur-Mulasysla";
        case 21:
            return "Nordur-Tingeyjarsysla";
        case 23:
            return "Rangarvallasysla";
        case 28:
            return "Skagafjardarsysla";
        case 29:
            return "Snafellsnes- og Hnappadalssysla";
        case 30:
            return "Strandasysla";
        case 31:
            return "Sudur-Mulasysla";
        case 32:
            return "Sudur-Tingeyjarsysla";
        case 34:
            return "Vestur-Bardastrandarsysla";
        case 35:
            return "Vestur-Hunavatnssysla";
        case 36:
            return "Vestur-Isafjardarsysla";
        case 37:
            return "Vestur-Skaftafellssysla";
        case 38:
            return "Austurland";
        case 39:
            return "Hofuoborgarsvaoio";
        case 40:
            return "Norourland Eystra";
        case 41:
            return "Norourland Vestra";
        case 42:
            return "Suourland";
        case 43:
            return "Suournes";
        case 44:
            return "Vestfiroir";
        case 45:
            return "Vesturland";
        }
    }else if (strcmp(country_code, "IT") == 0) {
        switch (region_code2) {
        case 1:
            return "Abruzzi";
        case 2:
            return "Basilicata";
        case 3:
            return "Calabria";
        case 4:
            return "Campania";
        case 5:
            return "Emilia-Romagna";
        case 6:
            return "Friuli-Venezia Giulia";
        case 7:
            return "Lazio";
        case 8:
            return "Liguria";
        case 9:
            return "Lombardia";
        case 10:
            return "Marche";
        case 11:
            return "Molise";
        case 12:
            return "Piemonte";
        case 13:
            return "Puglia";
        case 14:
            return "Sardegna";
        case 15:
            return "Sicilia";
        case 16:
            return "Toscana";
        case 17:
            return "Trentino-Alto Adige";
        case 18:
            return "Umbria";
        case 19:
            return "Valle d'Aosta";
        case 20:
            return "Veneto";
        }
    }else if (strcmp(country_code, "JM") == 0) {
        switch (region_code2) {
        case 1:
            return "Clarendon";
        case 2:
            return "Hanover";
        case 4:
            return "Manchester";
        case 7:
            return "Portland";
        case 8:
            return "Saint Andrew";
        case 9:
            return "Saint Ann";
        case 10:
            return "Saint Catherine";
        case 11:
            return "Saint Elizabeth";
        case 12:
            return "Saint James";
        case 13:
            return "Saint Mary";
        case 14:
            return "Saint Thomas";
        case 15:
            return "Trelawny";
        case 16:
            return "Westmoreland";
        case 17:
            return "Kingston";
        }
    }else if (strcmp(country_code, "JO") == 0) {
        switch (region_code2) {
        case 2:
            return "Al Balqa'";
        case 9:
            return "Al Karak";
        case 12:
            return "At Tafilah";
        case 15:
            return "Al Mafraq";
        case 16:
            return "Amman";
        case 17:
            return "Az Zaraqa";
        case 18:
            return "Irbid";
        case 19:
            return "Ma'an";
        case 20:
            return "Ajlun";
        case 21:
            return "Al Aqabah";
        case 22:
            return "Jarash";
        case 23:
            return "Madaba";
        }
    }else if (strcmp(country_code, "JP") == 0) {
        switch (region_code2) {
        case 1:
            return "Aichi";
        case 2:
            return "Akita";
        case 3:
            return "Aomori";
        case 4:
            return "Chiba";
        case 5:
            return "Ehime";
        case 6:
            return "Fukui";
        case 7:
            return "Fukuoka";
        case 8:
            return "Fukushima";
        case 9:
            return "Gifu";
        case 10:
            return "Gumma";
        case 11:
            return "Hiroshima";
        case 12:
            return "Hokkaido";
        case 13:
            return "Hyogo";
        case 14:
            return "Ibaraki";
        case 15:
            return "Ishikawa";
        case 16:
            return "Iwate";
        case 17:
            return "Kagawa";
        case 18:
            return "Kagoshima";
        case 19:
            return "Kanagawa";
        case 20:
            return "Kochi";
        case 21:
            return "Kumamoto";
        case 22:
            return "Kyoto";
        case 23:
            return "Mie";
        case 24:
            return "Miyagi";
        case 25:
            return "Miyazaki";
        case 26:
            return "Nagano";
        case 27:
            return "Nagasaki";
        case 28:
            return "Nara";
        case 29:
            return "Niigata";
        case 30:
            return "Oita";
        case 31:
            return "Okayama";
        case 32:
            return "Osaka";
        case 33:
            return "Saga";
        case 34:
            return "Saitama";
        case 35:
            return "Shiga";
        case 36:
            return "Shimane";
        case 37:
            return "Shizuoka";
        case 38:
            return "Tochigi";
        case 39:
            return "Tokushima";
        case 40:
            return "Tokyo";
        case 41:
            return "Tottori";
        case 42:
            return "Toyama";
        case 43:
            return "Wakayama";
        case 44:
            return "Yamagata";
        case 45:
            return "Yamaguchi";
        case 46:
            return "Yamanashi";
        case 47:
            return "Okinawa";
        }
    }else if (strcmp(country_code, "KE") == 0) {
        switch (region_code2) {
        case 1:
            return "Central";
        case 2:
            return "Coast";
        case 3:
            return "Eastern";
        case 5:
            return "Nairobi Area";
        case 6:
            return "North-Eastern";
        case 7:
            return "Nyanza";
        case 8:
            return "Rift Valley";
        case 9:
            return "Western";
        }
    }else if (strcmp(country_code, "KG") == 0) {
        switch (region_code2) {
        case 1:
            return "Bishkek";
        case 2:
            return "Chuy";
        case 3:
            return "Jalal-Abad";
        case 4:
            return "Naryn";
        case 5:
            return "Osh";
        case 6:
            return "Talas";
        case 7:
            return "Ysyk-Kol";
        case 8:
            return "Osh";
        case 9:
            return "Batken";
        }
    }else if (strcmp(country_code, "KH") == 0) {
        switch (region_code2) {
        case 1:
            return "Batdambang";
        case 2:
            return "Kampong Cham";
        case 3:
            return "Kampong Chhnang";
        case 4:
            return "Kampong Speu";
        case 5:
            return "Kampong Thum";
        case 6:
            return "Kampot";
        case 7:
            return "Kandal";
        case 8:
            return "Koh Kong";
        case 9:
            return "Kracheh";
        case 10:
            return "Mondulkiri";
        case 11:
            return "Phnum Penh";
        case 12:
            return "Pursat";
        case 13:
            return "Preah Vihear";
        case 14:
            return "Prey Veng";
        case 15:
            return "Ratanakiri Kiri";
        case 16:
            return "Siem Reap";
        case 17:
            return "Stung Treng";
        case 18:
            return "Svay Rieng";
        case 19:
            return "Takeo";
        case 22:
            return "Phnum Penh";
        case 25:
            return "Banteay Meanchey";
        case 28:
            return "Preah Seihanu";
        case 29:
            return "Batdambang";
        case 30:
            return "Pailin";
        }
    }else if (strcmp(country_code, "KI") == 0) {
        switch (region_code2) {
        case 1:
            return "Gilbert Islands";
        case 2:
            return "Line Islands";
        case 3:
            return "Phoenix Islands";
        }
    }else if (strcmp(country_code, "KM") == 0) {
        switch (region_code2) {
        case 1:
            return "Anjouan";
        case 2:
            return "Grande Comore";
        case 3:
            return "Moheli";
        }
    }else if (strcmp(country_code, "KN") == 0) {
        switch (region_code2) {
        case 1:
            return "Christ Church Nichola Town";
        case 2:
            return "Saint Anne Sandy Point";
        case 3:
            return "Saint George Basseterre";
        case 4:
            return "Saint George Gingerland";
        case 5:
            return "Saint James Windward";
        case 6:
            return "Saint John Capisterre";
        case 7:
            return "Saint John Figtree";
        case 8:
            return "Saint Mary Cayon";
        case 9:
            return "Saint Paul Capisterre";
        case 10:
            return "Saint Paul Charlestown";
        case 11:
            return "Saint Peter Basseterre";
        case 12:
            return "Saint Thomas Lowland";
        case 13:
            return "Saint Thomas Middle Island";
        case 15:
            return "Trinity Palmetto Point";
        }
    }else if (strcmp(country_code, "KP") == 0) {
        switch (region_code2) {
        case 1:
            return "Chagang-do";
        case 3:
            return "Hamgyong-namdo";
        case 6:
            return "Hwanghae-namdo";
        case 7:
            return "Hwanghae-bukto";
        case 8:
            return "Kaesong-si";
        case 9:
            return "Kangwon-do";
        case 11:
            return "P'yongan-bukto";
        case 12:
            return "P'yongyang-si";
        case 13:
            return "Yanggang-do";
        case 14:
            return "Namp'o-si";
        case 15:
            return "P'yongan-namdo";
        case 17:
            return "Hamgyong-bukto";
        case 18:
            return "Najin Sonbong-si";
        }
    }else if (strcmp(country_code, "KR") == 0) {
        switch (region_code2) {
        case 1:
            return "Cheju-do";
        case 3:
            return "Cholla-bukto";
        case 5:
            return "Ch'ungch'ong-bukto";
        case 6:
            return "Kangwon-do";
        case 10:
            return "Pusan-jikhalsi";
        case 11:
            return "Seoul-t'ukpyolsi";
        case 12:
            return "Inch'on-jikhalsi";
        case 13:
            return "Kyonggi-do";
        case 14:
            return "Kyongsang-bukto";
        case 15:
            return "Taegu-jikhalsi";
        case 16:
            return "Cholla-namdo";
        case 17:
            return "Ch'ungch'ong-namdo";
        case 18:
            return "Kwangju-jikhalsi";
        case 19:
            return "Taejon-jikhalsi";
        case 20:
            return "Kyongsang-namdo";
        case 21:
            return "Ulsan-gwangyoksi";
        }
    }else if (strcmp(country_code, "KW") == 0) {
        switch (region_code2) {
        case 1:
            return "Al Ahmadi";
        case 2:
            return "Al Kuwayt";
        case 5:
            return "Al Jahra";
        case 7:
            return "Al Farwaniyah";
        case 8:
            return "Hawalli";
        case 9:
            return "Mubarak al Kabir";
        }
    }else if (strcmp(country_code, "KY") == 0) {
        switch (region_code2) {
        case 1:
            return "Creek";
        case 2:
            return "Eastern";
        case 3:
            return "Midland";
        case 4:
            return "South Town";
        case 5:
            return "Spot Bay";
        case 6:
            return "Stake Bay";
        case 7:
            return "West End";
        case 8:
            return "Western";
        }
    }else if (strcmp(country_code, "KZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Almaty";
        case 2:
            return "Almaty City";
        case 3:
            return "Aqmola";
        case 4:
            return "Aqtobe";
        case 5:
            return "Astana";
        case 6:
            return "Atyrau";
        case 7:
            return "West Kazakhstan";
        case 8:
            return "Bayqonyr";
        case 9:
            return "Mangghystau";
        case 10:
            return "South Kazakhstan";
        case 11:
            return "Pavlodar";
        case 12:
            return "Qaraghandy";
        case 13:
            return "Qostanay";
        case 14:
            return "Qyzylorda";
        case 15:
            return "East Kazakhstan";
        case 16:
            return "North Kazakhstan";
        case 17:
            return "Zhambyl";
        }
    }else if (strcmp(country_code, "LA") == 0) {
        switch (region_code2) {
        case 1:
            return "Attapu";
        case 2:
            return "Champasak";
        case 3:
            return "Houaphan";
        case 4:
            return "Khammouan";
        case 5:
            return "Louang Namtha";
        case 7:
            return "Oudomxai";
        case 8:
            return "Phongsali";
        case 9:
            return "Saravan";
        case 10:
            return "Savannakhet";
        case 11:
            return "Vientiane";
        case 13:
            return "Xaignabouri";
        case 14:
            return "Xiangkhoang";
        case 17:
            return "Louangphrabang";
        }
    }else if (strcmp(country_code, "LB") == 0) {
        switch (region_code2) {
        case 1:
            return "Beqaa";
        case 2:
            return "Al Janub";
        case 3:
            return "Liban-Nord";
        case 4:
            return "Beyrouth";
        case 5:
            return "Mont-Liban";
        case 6:
            return "Liban-Sud";
        case 7:
            return "Nabatiye";
        case 8:
            return "Beqaa";
        case 9:
            return "Liban-Nord";
        case 10:
            return "Aakk,r";
        case 11:
            return "Baalbek-Hermel";
        }
    }else if (strcmp(country_code, "LC") == 0) {
        switch (region_code2) {
        case 1:
            return "Anse-la-Raye";
        case 2:
            return "Dauphin";
        case 3:
            return "Castries";
        case 4:
            return "Choiseul";
        case 5:
            return "Dennery";
        case 6:
            return "Gros-Islet";
        case 7:
            return "Laborie";
        case 8:
            return "Micoud";
        case 9:
            return "Soufriere";
        case 10:
            return "Vieux-Fort";
        case 11:
            return "Praslin";
        }
    }else if (strcmp(country_code, "LI") == 0) {
        switch (region_code2) {
        case 1:
            return "Balzers";
        case 2:
            return "Eschen";
        case 3:
            return "Gamprin";
        case 4:
            return "Mauren";
        case 5:
            return "Planken";
        case 6:
            return "Ruggell";
        case 7:
            return "Schaan";
        case 8:
            return "Schellenberg";
        case 9:
            return "Triesen";
        case 10:
            return "Triesenberg";
        case 11:
            return "Vaduz";
        case 21:
            return "Gbarpolu";
        case 22:
            return "River Gee";
        }
    }else if (strcmp(country_code, "LK") == 0) {
        switch (region_code2) {
        case 29:
            return "Central";
        case 30:
            return "North Central";
        case 32:
            return "North Western";
        case 33:
            return "Sabaragamuwa";
        case 34:
            return "Southern";
        case 35:
            return "Uva";
        case 36:
            return "Western";
        case 37:
            return "Eastern";
        case 38:
            return "Northern";
        }
    }else if (strcmp(country_code, "LR") == 0) {
        switch (region_code2) {
        case 1:
            return "Bong";
        case 4:
            return "Grand Cape Mount";
        case 5:
            return "Lofa";
        case 6:
            return "Maryland";
        case 7:
            return "Monrovia";
        case 9:
            return "Nimba";
        case 10:
            return "Sino";
        case 11:
            return "Grand Bassa";
        case 12:
            return "Grand Cape Mount";
        case 13:
            return "Maryland";
        case 14:
            return "Montserrado";
        case 17:
            return "Margibi";
        case 18:
            return "River Cess";
        case 19:
            return "Grand Gedeh";
        case 20:
            return "Lofa";
        case 21:
            return "Gbarpolu";
        case 22:
            return "River Gee";
        }
    }else if (strcmp(country_code, "LS") == 0) {
        switch (region_code2) {
        case 10:
            return "Berea";
        case 11:
            return "Butha-Buthe";
        case 12:
            return "Leribe";
        case 13:
            return "Mafeteng";
        case 14:
            return "Maseru";
        case 15:
            return "Mohales Hoek";
        case 16:
            return "Mokhotlong";
        case 17:
            return "Qachas Nek";
        case 18:
            return "Quthing";
        case 19:
            return "Thaba-Tseka";
        }
    }else if (strcmp(country_code, "LT") == 0) {
        switch (region_code2) {
        case 56:
            return "Alytaus Apskritis";
        case 57:
            return "Kauno Apskritis";
        case 58:
            return "Klaipedos Apskritis";
        case 59:
            return "Marijampoles Apskritis";
        case 60:
            return "Panevezio Apskritis";
        case 61:
            return "Siauliu Apskritis";
        case 62:
            return "Taurages Apskritis";
        case 63:
            return "Telsiu Apskritis";
        case 64:
            return "Utenos Apskritis";
        case 65:
            return "Vilniaus Apskritis";
        }
    }else if (strcmp(country_code, "LU") == 0) {
        switch (region_code2) {
        case 1:
            return "Diekirch";
        case 2:
            return "Grevenmacher";
        case 3:
            return "Luxembourg";
        }
    }else if (strcmp(country_code, "LV") == 0) {
        switch (region_code2) {
        case 1:
            return "Aizkraukles";
        case 2:
            return "Aluksnes";
        case 3:
            return "Balvu";
        case 4:
            return "Bauskas";
        case 5:
            return "Cesu";
        case 6:
            return "Daugavpils";
        case 7:
            return "Daugavpils";
        case 8:
            return "Dobeles";
        case 9:
            return "Gulbenes";
        case 10:
            return "Jekabpils";
        case 11:
            return "Jelgava";
        case 12:
            return "Jelgavas";
        case 13:
            return "Jurmala";
        case 14:
            return "Kraslavas";
        case 15:
            return "Kuldigas";
        case 16:
            return "Liepaja";
        case 17:
            return "Liepajas";
        case 18:
            return "Limbazu";
        case 19:
            return "Ludzas";
        case 20:
            return "Madonas";
        case 21:
            return "Ogres";
        case 22:
            return "Preilu";
        case 23:
            return "Rezekne";
        case 24:
            return "Rezeknes";
        case 25:
            return "Riga";
        case 26:
            return "Rigas";
        case 27:
            return "Saldus";
        case 28:
            return "Talsu";
        case 29:
            return "Tukuma";
        case 30:
            return "Valkas";
        case 31:
            return "Valmieras";
        case 32:
            return "Ventspils";
        case 33:
            return "Ventspils";
        }
    }else if (strcmp(country_code, "LY") == 0) {
        switch (region_code2) {
        case 3:
            return "Al Aziziyah";
        case 5:
            return "Al Jufrah";
        case 8:
            return "Al Kufrah";
        case 13:
            return "Ash Shati'";
        case 30:
            return "Murzuq";
        case 34:
            return "Sabha";
        case 41:
            return "Tarhunah";
        case 42:
            return "Tubruq";
        case 45:
            return "Zlitan";
        case 47:
            return "Ajdabiya";
        case 48:
            return "Al Fatih";
        case 49:
            return "Al Jabal al Akhdar";
        case 50:
            return "Al Khums";
        case 51:
            return "An Nuqat al Khams";
        case 52:
            return "Awbari";
        case 53:
            return "Az Zawiyah";
        case 54:
            return "Banghazi";
        case 55:
            return "Darnah";
        case 56:
            return "Ghadamis";
        case 57:
            return "Gharyan";
        case 58:
            return "Misratah";
        case 59:
            return "Sawfajjin";
        case 60:
            return "Surt";
        case 61:
            return "Tarabulus";
        case 62:
            return "Yafran";
        }
    }else if (strcmp(country_code, "MA") == 0) {
        switch (region_code2) {
        case 45:
            return "Grand Casablanca";
        case 46:
            return "Fes-Boulemane";
        case 47:
            return "Marrakech-Tensift-Al Haouz";
        case 48:
            return "Meknes-Tafilalet";
        case 49:
            return "Rabat-Sale-Zemmour-Zaer";
        case 50:
            return "Chaouia-Ouardigha";
        case 51:
            return "Doukkala-Abda";
        case 52:
            return "Gharb-Chrarda-Beni Hssen";
        case 53:
            return "Guelmim-Es Smara";
        case 54:
            return "Oriental";
        case 55:
            return "Souss-Massa-Dr,a";
        case 56:
            return "Tadla-Azilal";
        case 57:
            return "Tanger-Tetouan";
        case 58:
            return "Taza-Al Hoceima-Taounate";
        case 59:
            return "La,youne-Boujdour-Sakia El Hamra";
        }
    }else if (strcmp(country_code, "MC") == 0) {
        switch (region_code2) {
        case 1:
            return "La Condamine";
        case 2:
            return "Monaco";
        case 3:
            return "Monte-Carlo";
        }
    }else if (strcmp(country_code, "MD") == 0) {
        switch (region_code2) {
        case 51:
            return "Gagauzia";
        case 57:
            return "Chisinau";
        case 58:
            return "Stinga Nistrului";
        case 59:
            return "Anenii Noi";
        case 60:
            return "Balti";
        case 61:
            return "Basarabeasca";
        case 62:
            return "Bender";
        case 63:
            return "Briceni";
        case 64:
            return "Cahul";
        case 65:
            return "Cantemir";
        case 66:
            return "Calarasi";
        case 67:
            return "Causeni";
        case 68:
            return "Cimislia";
        case 69:
            return "Criuleni";
        case 70:
            return "Donduseni";
        case 71:
            return "Drochia";
        case 72:
            return "Dubasari";
        case 73:
            return "Edinet";
        case 74:
            return "Falesti";
        case 75:
            return "Floresti";
        case 76:
            return "Glodeni";
        case 77:
            return "Hincesti";
        case 78:
            return "Ialoveni";
        case 79:
            return "Leova";
        case 80:
            return "Nisporeni";
        case 81:
            return "Ocnita";
        case 82:
            return "Orhei";
        case 83:
            return "Rezina";
        case 84:
            return "Riscani";
        case 85:
            return "Singerei";
        case 86:
            return "Soldanesti";
        case 87:
            return "Soroca";
        case 88:
            return "Stefan-Voda";
        case 89:
            return "Straseni";
        case 90:
            return "Taraclia";
        case 91:
            return "Telenesti";
        case 92:
            return "Ungheni";
        }
    }else if (strcmp(country_code, "MG") == 0) {
        switch (region_code2) {
        case 1:
            return "Antsiranana";
        case 2:
            return "Fianarantsoa";
        case 3:
            return "Mahajanga";
        case 4:
            return "Toamasina";
        case 5:
            return "Antananarivo";
        case 6:
            return "Toliara";
        }
    }else if (strcmp(country_code, "MK") == 0) {
        switch (region_code2) {
        case 1:
            return "Aracinovo";
        case 2:
            return "Bac";
        case 3:
            return "Belcista";
        case 4:
            return "Berovo";
        case 5:
            return "Bistrica";
        case 6:
            return "Bitola";
        case 7:
            return "Blatec";
        case 8:
            return "Bogdanci";
        case 9:
            return "Bogomila";
        case 10:
            return "Bogovinje";
        case 11:
            return "Bosilovo";
        case 12:
            return "Brvenica";
        case 13:
            return "Cair";
        case 14:
            return "Capari";
        case 15:
            return "Caska";
        case 16:
            return "Cegrane";
        case 17:
            return "Centar";
        case 18:
            return "Centar Zupa";
        case 19:
            return "Cesinovo";
        case 20:
            return "Cucer-Sandevo";
        case 21:
            return "Debar";
        case 22:
            return "Delcevo";
        case 23:
            return "Delogozdi";
        case 24:
            return "Demir Hisar";
        case 25:
            return "Demir Kapija";
        case 26:
            return "Dobrusevo";
        case 27:
            return "Dolna Banjica";
        case 28:
            return "Dolneni";
        case 29:
            return "Dorce Petrov";
        case 30:
            return "Drugovo";
        case 31:
            return "Dzepciste";
        case 32:
            return "Gazi Baba";
        case 33:
            return "Gevgelija";
        case 34:
            return "Gostivar";
        case 35:
            return "Gradsko";
        case 36:
            return "Ilinden";
        case 37:
            return "Izvor";
        case 38:
            return "Jegunovce";
        case 39:
            return "Kamenjane";
        case 40:
            return "Karbinci";
        case 41:
            return "Karpos";
        case 42:
            return "Kavadarci";
        case 43:
            return "Kicevo";
        case 44:
            return "Kisela Voda";
        case 45:
            return "Klecevce";
        case 46:
            return "Kocani";
        case 47:
            return "Konce";
        case 48:
            return "Kondovo";
        case 49:
            return "Konopiste";
        case 50:
            return "Kosel";
        case 51:
            return "Kratovo";
        case 52:
            return "Kriva Palanka";
        case 53:
            return "Krivogastani";
        case 54:
            return "Krusevo";
        case 55:
            return "Kuklis";
        case 56:
            return "Kukurecani";
        case 57:
            return "Kumanovo";
        case 58:
            return "Labunista";
        case 59:
            return "Lipkovo";
        case 60:
            return "Lozovo";
        case 61:
            return "Lukovo";
        case 62:
            return "Makedonska Kamenica";
        case 63:
            return "Makedonski Brod";
        case 64:
            return "Mavrovi Anovi";
        case 65:
            return "Meseista";
        case 66:
            return "Miravci";
        case 67:
            return "Mogila";
        case 68:
            return "Murtino";
        case 69:
            return "Negotino";
        case 70:
            return "Negotino-Polosko";
        case 71:
            return "Novaci";
        case 72:
            return "Novo Selo";
        case 73:
            return "Oblesevo";
        case 74:
            return "Ohrid";
        case 75:
            return "Orasac";
        case 76:
            return "Orizari";
        case 77:
            return "Oslomej";
        case 78:
            return "Pehcevo";
        case 79:
            return "Petrovec";
        case 80:
            return "Plasnica";
        case 81:
            return "Podares";
        case 82:
            return "Prilep";
        case 83:
            return "Probistip";
        case 84:
            return "Radovis";
        case 85:
            return "Rankovce";
        case 86:
            return "Resen";
        case 87:
            return "Rosoman";
        case 88:
            return "Rostusa";
        case 89:
            return "Samokov";
        case 90:
            return "Saraj";
        case 91:
            return "Sipkovica";
        case 92:
            return "Sopiste";
        case 93:
            return "Sopotnica";
        case 94:
            return "Srbinovo";
        case 95:
            return "Staravina";
        case 96:
            return "Star Dojran";
        case 97:
            return "Staro Nagoricane";
        case 98:
            return "Stip";
        case 99:
            return "Struga";
        case 832:
            return "Strumica";
        case 833:
            return "Studenicani";
        case 834:
            return "Suto Orizari";
        case 835:
            return "Sveti Nikole";
        case 836:
            return "Tearce";
        case 837:
            return "Tetovo";
        case 838:
            return "Topolcani";
        case 839:
            return "Valandovo";
        case 840:
            return "Vasilevo";
        case 875:
            return "Veles";
        case 876:
            return "Velesta";
        case 877:
            return "Vevcani";
        case 878:
            return "Vinica";
        case 879:
            return "Vitoliste";
        case 880:
            return "Vranestica";
        case 881:
            return "Vrapciste";
        case 882:
            return "Vratnica";
        case 883:
            return "Vrutok";
        case 918:
            return "Zajas";
        case 919:
            return "Zelenikovo";
        case 920:
            return "Zelino";
        case 921:
            return "Zitose";
        case 922:
            return "Zletovo";
        case 923:
            return "Zrnovci";
        case 925:
            return "Cair";
        case 926:
            return "Caska";
        case 962:
            return "Debar";
        case 963:
            return "Demir Hisar";
        case 964:
            return "Gostivar";
        case 966:
            return "Kavadarci";
        case 967:
            return "Kumanovo";
        case 968:
            return "Makedonski Brod";
        case 1005:
            return "Ohrid";
        case 1006:
            return "Prilep";
        case 1008:
            return "Dojran";
        case 1009:
            return "Struga";
        case 1010:
            return "Strumica";
        case 1011:
            return "Tetovo";
        case 1012:
            return "Valandovo";
        case 1047:
            return "Veles";
        case 1048:
            return "Aerodrom";
        }
    }else if (strcmp(country_code, "ML") == 0) {
        switch (region_code2) {
        case 1:
            return "Bamako";
        case 3:
            return "Kayes";
        case 4:
            return "Mopti";
        case 5:
            return "Segou";
        case 6:
            return "Sikasso";
        case 7:
            return "Koulikoro";
        case 8:
            return "Tombouctou";
        case 9:
            return "Gao";
        case 10:
            return "Kidal";
        }
    }else if (strcmp(country_code, "MM") == 0) {
        switch (region_code2) {
        case 1:
            return "Rakhine State";
        case 2:
            return "Chin State";
        case 3:
            return "Irrawaddy";
        case 4:
            return "Kachin State";
        case 5:
            return "Karan State";
        case 6:
            return "Kayah State";
        case 7:
            return "Magwe";
        case 8:
            return "Mandalay";
        case 9:
            return "Pegu";
        case 10:
            return "Sagaing";
        case 11:
            return "Shan State";
        case 12:
            return "Tenasserim";
        case 13:
            return "Mon State";
        case 14:
            return "Rangoon";
        case 17:
            return "Yangon";
        }
    }else if (strcmp(country_code, "MN") == 0) {
        switch (region_code2) {
        case 1:
            return "Arhangay";
        case 2:
            return "Bayanhongor";
        case 3:
            return "Bayan-Olgiy";
        case 5:
            return "Darhan";
        case 6:
            return "Dornod";
        case 7:
            return "Dornogovi";
        case 8:
            return "Dundgovi";
        case 9:
            return "Dzavhan";
        case 10:
            return "Govi-Altay";
        case 11:
            return "Hentiy";
        case 12:
            return "Hovd";
        case 13:
            return "Hovsgol";
        case 14:
            return "Omnogovi";
        case 15:
            return "Ovorhangay";
        case 16:
            return "Selenge";
        case 17:
            return "Suhbaatar";
        case 18:
            return "Tov";
        case 19:
            return "Uvs";
        case 20:
            return "Ulaanbaatar";
        case 21:
            return "Bulgan";
        case 22:
            return "Erdenet";
        case 23:
            return "Darhan-Uul";
        case 24:
            return "Govisumber";
        case 25:
            return "Orhon";
        }
    }else if (strcmp(country_code, "MO") == 0) {
        switch (region_code2) {
        case 1:
            return "Ilhas";
        case 2:
            return "Macau";
        }
    }else if (strcmp(country_code, "MR") == 0) {
        switch (region_code2) {
        case 1:
            return "Hodh Ech Chargui";
        case 2:
            return "Hodh El Gharbi";
        case 3:
            return "Assaba";
        case 4:
            return "Gorgol";
        case 5:
            return "Brakna";
        case 6:
            return "Trarza";
        case 7:
            return "Adrar";
        case 8:
            return "Dakhlet Nouadhibou";
        case 9:
            return "Tagant";
        case 10:
            return "Guidimaka";
        case 11:
            return "Tiris Zemmour";
        case 12:
            return "Inchiri";
        }
    }else if (strcmp(country_code, "MS") == 0) {
        switch (region_code2) {
        case 1:
            return "Saint Anthony";
        case 2:
            return "Saint Georges";
        case 3:
            return "Saint Peter";
        }
    }else if (strcmp(country_code, "MU") == 0) {
        switch (region_code2) {
        case 12:
            return "Black River";
        case 13:
            return "Flacq";
        case 14:
            return "Grand Port";
        case 15:
            return "Moka";
        case 16:
            return "Pamplemousses";
        case 17:
            return "Plaines Wilhems";
        case 18:
            return "Port Louis";
        case 19:
            return "Riviere du Rempart";
        case 20:
            return "Savanne";
        case 21:
            return "Agalega Islands";
        case 22:
            return "Cargados Carajos";
        case 23:
            return "Rodrigues";
        }
    }else if (strcmp(country_code, "MV") == 0) {
        switch (region_code2) {
        case 1:
            return "Seenu";
        case 5:
            return "Laamu";
        case 30:
            return "Alifu";
        case 31:
            return "Baa";
        case 32:
            return "Dhaalu";
        case 33:
            return "Faafu ";
        case 34:
            return "Gaafu Alifu";
        case 35:
            return "Gaafu Dhaalu";
        case 36:
            return "Haa Alifu";
        case 37:
            return "Haa Dhaalu";
        case 38:
            return "Kaafu";
        case 39:
            return "Lhaviyani";
        case 40:
            return "Maale";
        case 41:
            return "Meemu";
        case 42:
            return "Gnaviyani";
        case 43:
            return "Noonu";
        case 44:
            return "Raa";
        case 45:
            return "Shaviyani";
        case 46:
            return "Thaa";
        case 47:
            return "Vaavu";
        }
    }else if (strcmp(country_code, "MW") == 0) {
        switch (region_code2) {
        case 2:
            return "Chikwawa";
        case 3:
            return "Chiradzulu";
        case 4:
            return "Chitipa";
        case 5:
            return "Thyolo";
        case 6:
            return "Dedza";
        case 7:
            return "Dowa";
        case 8:
            return "Karonga";
        case 9:
            return "Kasungu";
        case 11:
            return "Lilongwe";
        case 12:
            return "Mangochi";
        case 13:
            return "Mchinji";
        case 15:
            return "Mzimba";
        case 16:
            return "Ntcheu";
        case 17:
            return "Nkhata Bay";
        case 18:
            return "Nkhotakota";
        case 19:
            return "Nsanje";
        case 20:
            return "Ntchisi";
        case 21:
            return "Rumphi";
        case 22:
            return "Salima";
        case 23:
            return "Zomba";
        case 24:
            return "Blantyre";
        case 25:
            return "Mwanza";
        case 26:
            return "Balaka";
        case 27:
            return "Likoma";
        case 28:
            return "Machinga";
        case 29:
            return "Mulanje";
        case 30:
            return "Phalombe";
        }
    }else if (strcmp(country_code, "MX") == 0) {
        switch (region_code2) {
        case 1:
            return "Aguascalientes";
        case 2:
            return "Baja California";
        case 3:
            return "Baja California Sur";
        case 4:
            return "Campeche";
        case 5:
            return "Chiapas";
        case 6:
            return "Chihuahua";
        case 7:
            return "Coahuila de Zaragoza";
        case 8:
            return "Colima";
        case 9:
            return "Distrito Federal";
        case 10:
            return "Durango";
        case 11:
            return "Guanajuato";
        case 12:
            return "Guerrero";
        case 13:
            return "Hidalgo";
        case 14:
            return "Jalisco";
        case 15:
            return "Mexico";
        case 16:
            return "Michoacan de Ocampo";
        case 17:
            return "Morelos";
        case 18:
            return "Nayarit";
        case 19:
            return "Nuevo Leon";
        case 20:
            return "Oaxaca";
        case 21:
            return "Puebla";
        case 22:
            return "Queretaro de Arteaga";
        case 23:
            return "Quintana Roo";
        case 24:
            return "San Luis Potosi";
        case 25:
            return "Sinaloa";
        case 26:
            return "Sonora";
        case 27:
            return "Tabasco";
        case 28:
            return "Tamaulipas";
        case 29:
            return "Tlaxcala";
        case 30:
            return "Veracruz-Llave";
        case 31:
            return "Yucatan";
        case 32:
            return "Zacatecas";
        }
    }else if (strcmp(country_code, "MY") == 0) {
        switch (region_code2) {
        case 1:
            return "Johor";
        case 2:
            return "Kedah";
        case 3:
            return "Kelantan";
        case 4:
            return "Melaka";
        case 5:
            return "Negeri Sembilan";
        case 6:
            return "Pahang";
        case 7:
            return "Perak";
        case 8:
            return "Perlis";
        case 9:
            return "Pulau Pinang";
        case 11:
            return "Sarawak";
        case 12:
            return "Selangor";
        case 13:
            return "Terengganu";
        case 14:
            return "Kuala Lumpur";
        case 15:
            return "Labuan";
        case 16:
            return "Sabah";
        case 17:
            return "Putrajaya";
        }
    }else if (strcmp(country_code, "MZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Cabo Delgado";
        case 2:
            return "Gaza";
        case 3:
            return "Inhambane";
        case 4:
            return "Maputo";
        case 5:
            return "Sofala";
        case 6:
            return "Nampula";
        case 7:
            return "Niassa";
        case 8:
            return "Tete";
        case 9:
            return "Zambezia";
        case 10:
            return "Manica";
        case 11:
            return "Maputo";
        }
    }else if (strcmp(country_code, "NA") == 0) {
        switch (region_code2) {
        case 1:
            return "Bethanien";
        case 2:
            return "Caprivi Oos";
        case 3:
            return "Boesmanland";
        case 4:
            return "Gobabis";
        case 5:
            return "Grootfontein";
        case 6:
            return "Kaokoland";
        case 7:
            return "Karibib";
        case 8:
            return "Keetmanshoop";
        case 9:
            return "Luderitz";
        case 10:
            return "Maltahohe";
        case 11:
            return "Okahandja";
        case 12:
            return "Omaruru";
        case 13:
            return "Otjiwarongo";
        case 14:
            return "Outjo";
        case 15:
            return "Owambo";
        case 16:
            return "Rehoboth";
        case 17:
            return "Swakopmund";
        case 18:
            return "Tsumeb";
        case 20:
            return "Karasburg";
        case 21:
            return "Windhoek";
        case 22:
            return "Damaraland";
        case 23:
            return "Hereroland Oos";
        case 24:
            return "Hereroland Wes";
        case 25:
            return "Kavango";
        case 26:
            return "Mariental";
        case 27:
            return "Namaland";
        case 28:
            return "Caprivi";
        case 29:
            return "Erongo";
        case 30:
            return "Hardap";
        case 31:
            return "Karas";
        case 32:
            return "Kunene";
        case 33:
            return "Ohangwena";
        case 34:
            return "Okavango";
        case 35:
            return "Omaheke";
        case 36:
            return "Omusati";
        case 37:
            return "Oshana";
        case 38:
            return "Oshikoto";
        case 39:
            return "Otjozondjupa";
        }
    }else if (strcmp(country_code, "NE") == 0) {
        switch (region_code2) {
        case 1:
            return "Agadez";
        case 2:
            return "Diffa";
        case 3:
            return "Dosso";
        case 4:
            return "Maradi";
        case 5:
            return "Niamey";
        case 6:
            return "Tahoua";
        case 7:
            return "Zinder";
        case 8:
            return "Niamey";
        }
    }else if (strcmp(country_code, "NG") == 0) {
        switch (region_code2) {
        case 5:
            return "Lagos";
        case 11:
            return "Federal Capital Territory";
        case 16:
            return "Ogun";
        case 21:
            return "Akwa Ibom";
        case 22:
            return "Cross River";
        case 23:
            return "Kaduna";
        case 24:
            return "Katsina";
        case 25:
            return "Anambra";
        case 26:
            return "Benue";
        case 27:
            return "Borno";
        case 28:
            return "Imo";
        case 29:
            return "Kano";
        case 30:
            return "Kwara";
        case 31:
            return "Niger";
        case 32:
            return "Oyo";
        case 35:
            return "Adamawa";
        case 36:
            return "Delta";
        case 37:
            return "Edo";
        case 39:
            return "Jigawa";
        case 40:
            return "Kebbi";
        case 41:
            return "Kogi";
        case 42:
            return "Osun";
        case 43:
            return "Taraba";
        case 44:
            return "Yobe";
        case 45:
            return "Abia";
        case 46:
            return "Bauchi";
        case 47:
            return "Enugu";
        case 48:
            return "Ondo";
        case 49:
            return "Plateau";
        case 50:
            return "Rivers";
        case 51:
            return "Sokoto";
        case 52:
            return "Bayelsa";
        case 53:
            return "Ebonyi";
        case 54:
            return "Ekiti";
        case 55:
            return "Gombe";
        case 56:
            return "Nassarawa";
        case 57:
            return "Zamfara";
        }
    }else if (strcmp(country_code, "NI") == 0) {
        switch (region_code2) {
        case 1:
            return "Boaco";
        case 2:
            return "Carazo";
        case 3:
            return "Chinandega";
        case 4:
            return "Chontales";
        case 5:
            return "Esteli";
        case 6:
            return "Granada";
        case 7:
            return "Jinotega";
        case 8:
            return "Leon";
        case 9:
            return "Madriz";
        case 10:
            return "Managua";
        case 11:
            return "Masaya";
        case 12:
            return "Matagalpa";
        case 13:
            return "Nueva Segovia";
        case 14:
            return "Rio San Juan";
        case 15:
            return "Rivas";
        case 16:
            return "Zelaya";
        case 17:
            return "Autonoma Atlantico Norte";
        case 18:
            return "Region Autonoma Atlantico Sur";
        }
    }else if (strcmp(country_code, "NL") == 0) {
        switch (region_code2) {
        case 1:
            return "Drenthe";
        case 2:
            return "Friesland";
        case 3:
            return "Gelderland";
        case 4:
            return "Groningen";
        case 5:
            return "Limburg";
        case 6:
            return "Noord-Brabant";
        case 7:
            return "Noord-Holland";
        case 9:
            return "Utrecht";
        case 10:
            return "Zeeland";
        case 11:
            return "Zuid-Holland";
        case 15:
            return "Overijssel";
        case 16:
            return "Flevoland";
        }
    }else if (strcmp(country_code, "NO") == 0) {
        switch (region_code2) {
        case 1:
            return "Akershus";
        case 2:
            return "Aust-Agder";
        case 4:
            return "Buskerud";
        case 5:
            return "Finnmark";
        case 6:
            return "Hedmark";
        case 7:
            return "Hordaland";
        case 8:
            return "More og Romsdal";
        case 9:
            return "Nordland";
        case 10:
            return "Nord-Trondelag";
        case 11:
            return "Oppland";
        case 12:
            return "Oslo";
        case 13:
            return "Ostfold";
        case 14:
            return "Rogaland";
        case 15:
            return "Sogn og Fjordane";
        case 16:
            return "Sor-Trondelag";
        case 17:
            return "Telemark";
        case 18:
            return "Troms";
        case 19:
            return "Vest-Agder";
        case 20:
            return "Vestfold";
        }
    }else if (strcmp(country_code, "NP") == 0) {
        switch (region_code2) {
        case 1:
            return "Bagmati";
        case 2:
            return "Bheri";
        case 3:
            return "Dhawalagiri";
        case 4:
            return "Gandaki";
        case 5:
            return "Janakpur";
        case 6:
            return "Karnali";
        case 7:
            return "Kosi";
        case 8:
            return "Lumbini";
        case 9:
            return "Mahakali";
        case 10:
            return "Mechi";
        case 11:
            return "Narayani";
        case 12:
            return "Rapti";
        case 13:
            return "Sagarmatha";
        case 14:
            return "Seti";
        }
    }else if (strcmp(country_code, "NR") == 0) {
        switch (region_code2) {
        case 1:
            return "Aiwo";
        case 2:
            return "Anabar";
        case 3:
            return "Anetan";
        case 4:
            return "Anibare";
        case 5:
            return "Baiti";
        case 6:
            return "Boe";
        case 7:
            return "Buada";
        case 8:
            return "Denigomodu";
        case 9:
            return "Ewa";
        case 10:
            return "Ijuw";
        case 11:
            return "Meneng";
        case 12:
            return "Nibok";
        case 13:
            return "Uaboe";
        case 14:
            return "Yaren";
        }
    }else if (strcmp(country_code, "NZ") == 0) {
        switch (region_code2) {
        case 10:
            return "Chatham Islands";
        case 1010:
            return "Auckland";
        case 1011:
            return "Bay of Plenty";
        case 1012:
            return "Canterbury";
        case 1047:
            return "Gisborne";
        case 1048:
            return "Hawke's Bay";
        case 1049:
            return "Manawatu-Wanganui";
        case 1050:
            return "Marlborough";
        case 1051:
            return "Nelson";
        case 1052:
            return "Northland";
        case 1053:
            return "Otago";
        case 1054:
            return "Southland";
        case 1055:
            return "Taranaki";
        case 1090:
            return "Waikato";
        case 1091:
            return "Wellington";
        case 1092:
            return "West Coast";
        }
    }else if (strcmp(country_code, "OM") == 0) {
        switch (region_code2) {
        case 1:
            return "Ad Dakhiliyah";
        case 2:
            return "Al Batinah";
        case 3:
            return "Al Wusta";
        case 4:
            return "Ash Sharqiyah";
        case 5:
            return "Az Zahirah";
        case 6:
            return "Masqat";
        case 7:
            return "Musandam";
        case 8:
            return "Zufar";
        }
    }else if (strcmp(country_code, "PA") == 0) {
        switch (region_code2) {
        case 1:
            return "Bocas del Toro";
        case 2:
            return "Chiriqui";
        case 3:
            return "Cocle";
        case 4:
            return "Colon";
        case 5:
            return "Darien";
        case 6:
            return "Herrera";
        case 7:
            return "Los Santos";
        case 8:
            return "Panama";
        case 9:
            return "San Blas";
        case 10:
            return "Veraguas";
        }
    }else if (strcmp(country_code, "PE") == 0) {
        switch (region_code2) {
        case 1:
            return "Amazonas";
        case 2:
            return "Ancash";
        case 3:
            return "Apurimac";
        case 4:
            return "Arequipa";
        case 5:
            return "Ayacucho";
        case 6:
            return "Cajamarca";
        case 7:
            return "Callao";
        case 8:
            return "Cusco";
        case 9:
            return "Huancavelica";
        case 10:
            return "Huanuco";
        case 11:
            return "Ica";
        case 12:
            return "Junin";
        case 13:
            return "La Libertad";
        case 14:
            return "Lambayeque";
        case 15:
            return "Lima";
        case 16:
            return "Loreto";
        case 17:
            return "Madre de Dios";
        case 18:
            return "Moquegua";
        case 19:
            return "Pasco";
        case 20:
            return "Piura";
        case 21:
            return "Puno";
        case 22:
            return "San Martin";
        case 23:
            return "Tacna";
        case 24:
            return "Tumbes";
        case 25:
            return "Ucayali";
        }
    }else if (strcmp(country_code, "PG") == 0) {
        switch (region_code2) {
        case 1:
            return "Central";
        case 2:
            return "Gulf";
        case 3:
            return "Milne Bay";
        case 4:
            return "Northern";
        case 5:
            return "Southern Highlands";
        case 6:
            return "Western";
        case 7:
            return "North Solomons";
        case 8:
            return "Chimbu";
        case 9:
            return "Eastern Highlands";
        case 10:
            return "East New Britain";
        case 11:
            return "East Sepik";
        case 12:
            return "Madang";
        case 13:
            return "Manus";
        case 14:
            return "Morobe";
        case 15:
            return "New Ireland";
        case 16:
            return "Western Highlands";
        case 17:
            return "West New Britain";
        case 18:
            return "Sandaun";
        case 19:
            return "Enga";
        case 20:
            return "National Capital";
        }
    }else if (strcmp(country_code, "PH") == 0) {
        switch (region_code2) {
        case 1:
            return "Abra";
        case 2:
            return "Agusan del Norte";
        case 3:
            return "Agusan del Sur";
        case 4:
            return "Aklan";
        case 5:
            return "Albay";
        case 6:
            return "Antique";
        case 7:
            return "Bataan";
        case 8:
            return "Batanes";
        case 9:
            return "Batangas";
        case 10:
            return "Benguet";
        case 11:
            return "Bohol";
        case 12:
            return "Bukidnon";
        case 13:
            return "Bulacan";
        case 14:
            return "Cagayan";
        case 15:
            return "Camarines Norte";
        case 16:
            return "Camarines Sur";
        case 17:
            return "Camiguin";
        case 18:
            return "Capiz";
        case 19:
            return "Catanduanes";
        case 20:
            return "Cavite";
        case 21:
            return "Cebu";
        case 22:
            return "Basilan";
        case 23:
            return "Eastern Samar";
        case 24:
            return "Davao";
        case 25:
            return "Davao del Sur";
        case 26:
            return "Davao Oriental";
        case 27:
            return "Ifugao";
        case 28:
            return "Ilocos Norte";
        case 29:
            return "Ilocos Sur";
        case 30:
            return "Iloilo";
        case 31:
            return "Isabela";
        case 32:
            return "Kalinga-Apayao";
        case 33:
            return "Laguna";
        case 34:
            return "Lanao del Norte";
        case 35:
            return "Lanao del Sur";
        case 36:
            return "La Union";
        case 37:
            return "Leyte";
        case 38:
            return "Marinduque";
        case 39:
            return "Masbate";
        case 40:
            return "Mindoro Occidental";
        case 41:
            return "Mindoro Oriental";
        case 42:
            return "Misamis Occidental";
        case 43:
            return "Misamis Oriental";
        case 44:
            return "Mountain";
        case 45:
            return "Negros Occidental";
        case 46:
            return "Negros Oriental";
        case 47:
            return "Nueva Ecija";
        case 48:
            return "Nueva Vizcaya";
        case 49:
            return "Palawan";
        case 50:
            return "Pampanga";
        case 51:
            return "Pangasinan";
        case 53:
            return "Rizal";
        case 54:
            return "Romblon";
        case 55:
            return "Samar";
        case 56:
            return "Maguindanao";
        case 57:
            return "North Cotabato";
        case 58:
            return "Sorsogon";
        case 59:
            return "Southern Leyte";
        case 60:
            return "Sulu";
        case 61:
            return "Surigao del Norte";
        case 62:
            return "Surigao del Sur";
        case 63:
            return "Tarlac";
        case 64:
            return "Zambales";
        case 65:
            return "Zamboanga del Norte";
        case 66:
            return "Zamboanga del Sur";
        case 67:
            return "Northern Samar";
        case 68:
            return "Quirino";
        case 69:
            return "Siquijor";
        case 70:
            return "South Cotabato";
        case 71:
            return "Sultan Kudarat";
        case 72:
            return "Tawitawi";
        case 832:
            return "Angeles";
        case 833:
            return "Bacolod";
        case 834:
            return "Bago";
        case 835:
            return "Baguio";
        case 836:
            return "Bais";
        case 837:
            return "Basilan City";
        case 838:
            return "Batangas City";
        case 839:
            return "Butuan";
        case 840:
            return "Cabanatuan";
        case 875:
            return "Cadiz";
        case 876:
            return "Cagayan de Oro";
        case 877:
            return "Calbayog";
        case 878:
            return "Caloocan";
        case 879:
            return "Canlaon";
        case 880:
            return "Cavite City";
        case 881:
            return "Cebu City";
        case 882:
            return "Cotabato";
        case 883:
            return "Dagupan";
        case 918:
            return "Danao";
        case 919:
            return "Dapitan";
        case 920:
            return "Davao City";
        case 921:
            return "Dipolog";
        case 922:
            return "Dumaguete";
        case 923:
            return "General Santos";
        case 924:
            return "Gingoog";
        case 925:
            return "Iligan";
        case 926:
            return "Iloilo City";
        case 961:
            return "Iriga";
        case 962:
            return "La Carlota";
        case 963:
            return "Laoag";
        case 964:
            return "Lapu-Lapu";
        case 965:
            return "Legaspi";
        case 966:
            return "Lipa";
        case 967:
            return "Lucena";
        case 968:
            return "Mandaue";
        case 969:
            return "Manila";
        case 1004:
            return "Marawi";
        case 1005:
            return "Naga";
        case 1006:
            return "Olongapo";
        case 1007:
            return "Ormoc";
        case 1008:
            return "Oroquieta";
        case 1009:
            return "Ozamis";
        case 1010:
            return "Pagadian";
        case 1011:
            return "Palayan";
        case 1012:
            return "Pasay";
        case 1047:
            return "Puerto Princesa";
        case 1048:
            return "Quezon City";
        case 1049:
            return "Roxas";
        case 1050:
            return "San Carlos";
        case 1051:
            return "San Carlos";
        case 1052:
            return "San Jose";
        case 1053:
            return "San Pablo";
        case 1054:
            return "Silay";
        case 1055:
            return "Surigao";
        case 1090:
            return "Tacloban";
        case 1091:
            return "Tagaytay";
        case 1092:
            return "Tagbilaran";
        case 1093:
            return "Tangub";
        case 1094:
            return "Toledo";
        case 1095:
            return "Trece Martires";
        case 1096:
            return "Zamboanga";
        case 1097:
            return "Aurora";
        case 1134:
            return "Quezon";
        case 1135:
            return "Negros Occidental";
        case 1141:
            return "Biliran";
        case 1181:
            return "Compostela Valley";
        case 1182:
            return "Davao del Norte";
        case 1221:
            return "Guimaras";
        case 1222:
            return "Himamaylan";
        case 1225:
            return "Kalinga";
        case 1262:
            return "Las Pinas";
        case 1266:
            return "Malabon";
        case 1267:
            return "Malaybalay";
        case 1308:
            return "Muntinlupa";
        case 1309:
            return "Navotas";
        case 1311:
            return "Paranaque";
        case 1313:
            return "Passi";
        case 1477:
            return "Zambales";
        case 1352:
            return "San Jose del Monte";
        case 1353:
            return "San Juan";
        case 1355:
            return "Santiago";
        case 1356:
            return "Sarangani";
        case 1391:
            return "Sipalay";
        case 1393:
            return "Surigao del Norte";
        case 1478:
            return "Zamboanga";
        }
    }else if (strcmp(country_code, "PK") == 0) {
        switch (region_code2) {
        case 1:
            return "Federally Administered Tribal Areas";
        case 2:
            return "Balochistan";
        case 3:
            return "North-West Frontier";
        case 4:
            return "Punjab";
        case 5:
            return "Sindh";
        case 6:
            return "Azad Kashmir";
        case 7:
            return "Northern Areas";
        case 8:
            return "Islamabad";
        }
    }else if (strcmp(country_code, "PL") == 0) {
        switch (region_code2) {
        case 72:
            return "Dolnoslaskie";
        case 73:
            return "Kujawsko-Pomorskie";
        case 74:
            return "Lodzkie";
        case 75:
            return "Lubelskie";
        case 76:
            return "Lubuskie";
        case 77:
            return "Malopolskie";
        case 78:
            return "Mazowieckie";
        case 79:
            return "Opolskie";
        case 80:
            return "Podkarpackie";
        case 81:
            return "Podlaskie";
        case 82:
            return "Pomorskie";
        case 83:
            return "Slaskie";
        case 84:
            return "Swietokrzyskie";
        case 85:
            return "Warminsko-Mazurskie";
        case 86:
            return "Wielkopolskie";
        case 87:
            return "Zachodniopomorskie";
        }
    }else if (strcmp(country_code, "PS") == 0) {
        switch (region_code2) {
        case 1131:
            return "Gaza";
        case 1798:
            return "West Bank";
        }
    }else if (strcmp(country_code, "PT") == 0) {
        switch (region_code2) {
        case 2:
            return "Aveiro";
        case 3:
            return "Beja";
        case 4:
            return "Braga";
        case 5:
            return "Braganca";
        case 6:
            return "Castelo Branco";
        case 7:
            return "Coimbra";
        case 8:
            return "Evora";
        case 9:
            return "Faro";
        case 10:
            return "Madeira";
        case 11:
            return "Guarda";
        case 13:
            return "Leiria";
        case 14:
            return "Lisboa";
        case 16:
            return "Portalegre";
        case 17:
            return "Porto";
        case 18:
            return "Santarem";
        case 19:
            return "Setubal";
        case 20:
            return "Viana do Castelo";
        case 21:
            return "Vila Real";
        case 22:
            return "Viseu";
        case 23:
            return "Azores";
        }
    }else if (strcmp(country_code, "PY") == 0) {
        switch (region_code2) {
        case 1:
            return "Alto Parana";
        case 2:
            return "Amambay";
        case 4:
            return "Caaguazu";
        case 5:
            return "Caazapa";
        case 6:
            return "Central";
        case 7:
            return "Concepcion";
        case 8:
            return "Cordillera";
        case 10:
            return "Guaira";
        case 11:
            return "Itapua";
        case 12:
            return "Misiones";
        case 13:
            return "Neembucu";
        case 15:
            return "Paraguari";
        case 16:
            return "Presidente Hayes";
        case 17:
            return "San Pedro";
        case 19:
            return "Canindeyu";
        case 22:
            return "Asuncion";
        case 23:
            return "Alto Paraguay";
        case 24:
            return "Boqueron";
        }
    }else if (strcmp(country_code, "QA") == 0) {
        switch (region_code2) {
        case 1:
            return "Ad Dawhah";
        case 2:
            return "Al Ghuwariyah";
        case 3:
            return "Al Jumaliyah";
        case 4:
            return "Al Khawr";
        case 5:
            return "Al Wakrah Municipality";
        case 6:
            return "Ar Rayyan";
        case 8:
            return "Madinat ach Shamal";
        case 9:
            return "Umm Salal";
        case 10:
            return "Al Wakrah";
        case 11:
            return "Jariyan al Batnah";
        case 12:
            return "Umm Sa'id";
        }
    }else if (strcmp(country_code, "RO") == 0) {
        switch (region_code2) {
        case 1:
            return "Alba";
        case 2:
            return "Arad";
        case 3:
            return "Arges";
        case 4:
            return "Bacau";
        case 5:
            return "Bihor";
        case 6:
            return "Bistrita-Nasaud";
        case 7:
            return "Botosani";
        case 8:
            return "Braila";
        case 9:
            return "Brasov";
        case 10:
            return "Bucuresti";
        case 11:
            return "Buzau";
        case 12:
            return "Caras-Severin";
        case 13:
            return "Cluj";
        case 14:
            return "Constanta";
        case 15:
            return "Covasna";
        case 16:
            return "Dambovita";
        case 17:
            return "Dolj";
        case 18:
            return "Galati";
        case 19:
            return "Gorj";
        case 20:
            return "Harghita";
        case 21:
            return "Hunedoara";
        case 22:
            return "Ialomita";
        case 23:
            return "Iasi";
        case 25:
            return "Maramures";
        case 26:
            return "Mehedinti";
        case 27:
            return "Mures";
        case 28:
            return "Neamt";
        case 29:
            return "Olt";
        case 30:
            return "Prahova";
        case 31:
            return "Salaj";
        case 32:
            return "Satu Mare";
        case 33:
            return "Sibiu";
        case 34:
            return "Suceava";
        case 35:
            return "Teleorman";
        case 36:
            return "Timis";
        case 37:
            return "Tulcea";
        case 38:
            return "Vaslui";
        case 39:
            return "Valcea";
        case 40:
            return "Vrancea";
        case 41:
            return "Calarasi";
        case 42:
            return "Giurgiu";
        case 43:
            return "Ilfov";
        }
    }else if (strcmp(country_code, "RS") == 0) {
        switch (region_code2) {
        case 1:
            return "Kosovo";
        case 2:
            return "Vojvodina";
        }
    }else if (strcmp(country_code, "RU") == 0) {
        switch (region_code2) {
        case 1:
            return "Adygeya, Republic of";
        case 2:
            return "Aginsky Buryatsky AO";
        case 3:
            return "Gorno-Altay";
        case 4:
            return "Altaisky krai";
        case 5:
            return "Amur";
        case 6:
            return "Arkhangel'sk";
        case 7:
            return "Astrakhan'";
        case 8:
            return "Bashkortostan";
        case 9:
            return "Belgorod";
        case 10:
            return "Bryansk";
        case 11:
            return "Buryat";
        case 12:
            return "Chechnya";
        case 13:
            return "Chelyabinsk";
        case 14:
            return "Chita";
        case 15:
            return "Chukot";
        case 16:
            return "Chuvashia";
        case 17:
            return "Dagestan";
        case 18:
            return "Evenk";
        case 19:
            return "Ingush";
        case 20:
            return "Irkutsk";
        case 21:
            return "Ivanovo";
        case 22:
            return "Kabardin-Balkar";
        case 23:
            return "Kaliningrad";
        case 24:
            return "Kalmyk";
        case 25:
            return "Kaluga";
        case 26:
            return "Kamchatka";
        case 27:
            return "Karachay-Cherkess";
        case 28:
            return "Karelia";
        case 29:
            return "Kemerovo";
        case 30:
            return "Khabarovsk";
        case 31:
            return "Khakass";
        case 32:
            return "Khanty-Mansiy";
        case 33:
            return "Kirov";
        case 34:
            return "Komi";
        case 36:
            return "Koryak";
        case 37:
            return "Kostroma";
        case 38:
            return "Krasnodar";
        case 39:
            return "Krasnoyarsk";
        case 40:
            return "Kurgan";
        case 41:
            return "Kursk";
        case 42:
            return "Leningrad";
        case 43:
            return "Lipetsk";
        case 44:
            return "Magadan";
        case 45:
            return "Mariy-El";
        case 46:
            return "Mordovia";
        case 47:
            return "Moskva";
        case 48:
            return "Moscow City";
        case 49:
            return "Murmansk";
        case 50:
            return "Nenets";
        case 51:
            return "Nizhegorod";
        case 52:
            return "Novgorod";
        case 53:
            return "Novosibirsk";
        case 54:
            return "Omsk";
        case 55:
            return "Orenburg";
        case 56:
            return "Orel";
        case 57:
            return "Penza";
        case 58:
            return "Perm'";
        case 59:
            return "Primor'ye";
        case 60:
            return "Pskov";
        case 61:
            return "Rostov";
        case 62:
            return "Ryazan'";
        case 63:
            return "Sakha";
        case 64:
            return "Sakhalin";
        case 65:
            return "Samara";
        case 66:
            return "Saint Petersburg City";
        case 67:
            return "Saratov";
        case 68:
            return "North Ossetia";
        case 69:
            return "Smolensk";
        case 70:
            return "Stavropol'";
        case 71:
            return "Sverdlovsk";
        case 72:
            return "Tambovskaya oblast";
        case 73:
            return "Tatarstan";
        case 74:
            return "Taymyr";
        case 75:
            return "Tomsk";
        case 76:
            return "Tula";
        case 77:
            return "Tver'";
        case 78:
            return "Tyumen'";
        case 79:
            return "Tuva";
        case 80:
            return "Udmurt";
        case 81:
            return "Ul'yanovsk";
        case 83:
            return "Vladimir";
        case 84:
            return "Volgograd";
        case 85:
            return "Vologda";
        case 86:
            return "Voronezh";
        case 87:
            return "Yamal-Nenets";
        case 88:
            return "Yaroslavl'";
        case 89:
            return "Yevrey";
        case 90:
            return "Permskiy Kray";
        case 91:
            return "Krasnoyarskiy Kray";
        case 92:
            return "Kamchatskiy Kray";
        case 93:
            return "Zabaykal'skiy Kray";
        }
    }else if (strcmp(country_code, "RW") == 0) {
        switch (region_code2) {
        case 1:
            return "Butare";
        case 6:
            return "Gitarama";
        case 7:
            return "Kibungo";
        case 9:
            return "Kigali";
        case 11:
            return "Est";
        case 12:
            return "Kigali";
        case 13:
            return "Nord";
        case 14:
            return "Ouest";
        case 15:
            return "Sud";
        }
    }else if (strcmp(country_code, "SA") == 0) {
        switch (region_code2) {
        case 2:
            return "Al Bahah";
        case 5:
            return "Al Madinah";
        case 6:
            return "Ash Sharqiyah";
        case 8:
            return "Al Qasim";
        case 10:
            return "Ar Riyad";
        case 11:
            return "Asir Province";
        case 13:
            return "Ha'il";
        case 14:
            return "Makkah";
        case 15:
            return "Al Hudud ash Shamaliyah";
        case 16:
            return "Najran";
        case 17:
            return "Jizan";
        case 19:
            return "Tabuk";
        case 20:
            return "Al Jawf";
        }
    }else if (strcmp(country_code, "SB") == 0) {
        switch (region_code2) {
        case 3:
            return "Malaita";
        case 6:
            return "Guadalcanal";
        case 7:
            return "Isabel";
        case 8:
            return "Makira";
        case 9:
            return "Temotu";
        case 10:
            return "Central";
        case 11:
            return "Western";
        case 12:
            return "Choiseul";
        case 13:
            return "Rennell and Bellona";
        }
    }else if (strcmp(country_code, "SC") == 0) {
        switch (region_code2) {
        case 1:
            return "Anse aux Pins";
        case 2:
            return "Anse Boileau";
        case 3:
            return "Anse Etoile";
        case 4:
            return "Anse Louis";
        case 5:
            return "Anse Royale";
        case 6:
            return "Baie Lazare";
        case 7:
            return "Baie Sainte Anne";
        case 8:
            return "Beau Vallon";
        case 9:
            return "Bel Air";
        case 10:
            return "Bel Ombre";
        case 11:
            return "Cascade";
        case 12:
            return "Glacis";
        case 13:
            return "Grand' Anse";
        case 14:
            return "Grand' Anse";
        case 15:
            return "La Digue";
        case 16:
            return "La Riviere Anglaise";
        case 17:
            return "Mont Buxton";
        case 18:
            return "Mont Fleuri";
        case 19:
            return "Plaisance";
        case 20:
            return "Pointe La Rue";
        case 21:
            return "Port Glaud";
        case 22:
            return "Saint Louis";
        case 23:
            return "Takamaka";
        }
    }else if (strcmp(country_code, "SD") == 0) {
        switch (region_code2) {
        case 27:
            return "Al Wusta";
        case 28:
            return "Al Istiwa'iyah";
        case 29:
            return "Al Khartum";
        case 30:
            return "Ash Shamaliyah";
        case 31:
            return "Ash Sharqiyah";
        case 32:
            return "Bahr al Ghazal";
        case 33:
            return "Darfur";
        case 34:
            return "Kurdufan";
        case 35:
            return "Upper Nile";
        case 40:
            return "Al Wahadah State";
        case 44:
            return "Central Equatoria State";
        case 49:
            return "Southern Darfur";
        case 50:
            return "Southern Kordofan";
        case 52:
            return "Kassala";
        case 53:
            return "River Nile";
        case 55:
            return "Northern Darfur";
        }
    }else if (strcmp(country_code, "SE") == 0) {
        switch (region_code2) {
        case 2:
            return "Blekinge Lan";
        case 3:
            return "Gavleborgs Lan";
        case 5:
            return "Gotlands Lan";
        case 6:
            return "Hallands Lan";
        case 7:
            return "Jamtlands Lan";
        case 8:
            return "Jonkopings Lan";
        case 9:
            return "Kalmar Lan";
        case 10:
            return "Dalarnas Lan";
        case 12:
            return "Kronobergs Lan";
        case 14:
            return "Norrbottens Lan";
        case 15:
            return "Orebro Lan";
        case 16:
            return "Ostergotlands Lan";
        case 18:
            return "Sodermanlands Lan";
        case 21:
            return "Uppsala Lan";
        case 22:
            return "Varmlands Lan";
        case 23:
            return "Vasterbottens Lan";
        case 24:
            return "Vasternorrlands Lan";
        case 25:
            return "Vastmanlands Lan";
        case 26:
            return "Stockholms Lan";
        case 27:
            return "Skane Lan";
        case 28:
            return "Vastra Gotaland";
        }
    }else if (strcmp(country_code, "SH") == 0) {
        switch (region_code2) {
        case 1:
            return "Ascension";
        case 2:
            return "Saint Helena";
        case 3:
            return "Tristan da Cunha";
        }
    }else if (strcmp(country_code, "SI") == 0) {
        switch (region_code2) {
        case 1:
            return "Ajdovscina Commune";
        case 2:
            return "Beltinci Commune";
        case 3:
            return "Bled Commune";
        case 4:
            return "Bohinj Commune";
        case 5:
            return "Borovnica Commune";
        case 6:
            return "Bovec Commune";
        case 7:
            return "Brda Commune";
        case 8:
            return "Brezice Commune";
        case 9:
            return "Brezovica Commune";
        case 11:
            return "Celje Commune";
        case 12:
            return "Cerklje na Gorenjskem Commune";
        case 13:
            return "Cerknica Commune";
        case 14:
            return "Cerkno Commune";
        case 15:
            return "Crensovci Commune";
        case 16:
            return "Crna na Koroskem Commune";
        case 17:
            return "Crnomelj Commune";
        case 19:
            return "Divaca Commune";
        case 20:
            return "Dobrepolje Commune";
        case 22:
            return "Dol pri Ljubljani Commune";
        case 24:
            return "Dornava Commune";
        case 25:
            return "Dravograd Commune";
        case 26:
            return "Duplek Commune";
        case 27:
            return "Gorenja vas-Poljane Commune";
        case 28:
            return "Gorisnica Commune";
        case 29:
            return "Gornja Radgona Commune";
        case 30:
            return "Gornji Grad Commune";
        case 31:
            return "Gornji Petrovci Commune";
        case 32:
            return "Grosuplje Commune";
        case 34:
            return "Hrastnik Commune";
        case 35:
            return "Hrpelje-Kozina Commune";
        case 36:
            return "Idrija Commune";
        case 37:
            return "Ig Commune";
        case 38:
            return "Ilirska Bistrica Commune";
        case 39:
            return "Ivancna Gorica Commune";
        case 40:
            return "Izola-Isola Commune";
        case 42:
            return "Jursinci Commune";
        case 44:
            return "Kanal Commune";
        case 45:
            return "Kidricevo Commune";
        case 46:
            return "Kobarid Commune";
        case 47:
            return "Kobilje Commune";
        case 49:
            return "Komen Commune";
        case 50:
            return "Koper-Capodistria Urban Commune";
        case 51:
            return "Kozje Commune";
        case 52:
            return "Kranj Commune";
        case 53:
            return "Kranjska Gora Commune";
        case 54:
            return "Krsko Commune";
        case 55:
            return "Kungota Commune";
        case 57:
            return "Lasko Commune";
        case 61:
            return "Ljubljana Urban Commune";
        case 62:
            return "Ljubno Commune";
        case 64:
            return "Logatec Commune";
        case 66:
            return "Loski Potok Commune";
        case 68:
            return "Lukovica Commune";
        case 71:
            return "Medvode Commune";
        case 72:
            return "Menges Commune";
        case 73:
            return "Metlika Commune";
        case 74:
            return "Mezica Commune";
        case 76:
            return "Mislinja Commune";
        case 77:
            return "Moravce Commune";
        case 78:
            return "Moravske Toplice Commune";
        case 79:
            return "Mozirje Commune";
        case 80:
            return "Murska Sobota Urban Commune";
        case 81:
            return "Muta Commune";
        case 82:
            return "Naklo Commune";
        case 83:
            return "Nazarje Commune";
        case 84:
            return "Nova Gorica Urban Commune";
        case 86:
            return "Odranci Commune";
        case 87:
            return "Ormoz Commune";
        case 88:
            return "Osilnica Commune";
        case 89:
            return "Pesnica Commune";
        case 91:
            return "Pivka Commune";
        case 92:
            return "Podcetrtek Commune";
        case 94:
            return "Postojna Commune";
        case 97:
            return "Puconci Commune";
        case 98:
            return "Race-Fram Commune";
        case 99:
            return "Radece Commune";
        case 832:
            return "Radenci Commune";
        case 833:
            return "Radlje ob Dravi Commune";
        case 834:
            return "Radovljica Commune";
        case 837:
            return "Rogasovci Commune";
        case 838:
            return "Rogaska Slatina Commune";
        case 839:
            return "Rogatec Commune";
        case 875:
            return "Semic Commune";
        case 876:
            return "Sencur Commune";
        case 877:
            return "Sentilj Commune";
        case 878:
            return "Sentjernej Commune";
        case 880:
            return "Sevnica Commune";
        case 881:
            return "Sezana Commune";
        case 882:
            return "Skocjan Commune";
        case 883:
            return "Skofja Loka Commune";
        case 918:
            return "Skofljica Commune";
        case 919:
            return "Slovenj Gradec Urban Commune";
        case 921:
            return "Slovenske Konjice Commune";
        case 922:
            return "Smarje pri Jelsah Commune";
        case 923:
            return "Smartno ob Paki Commune";
        case 924:
            return "Sostanj Commune";
        case 925:
            return "Starse Commune";
        case 926:
            return "Store Commune";
        case 961:
            return "Sveti Jurij Commune";
        case 962:
            return "Tolmin Commune";
        case 963:
            return "Trbovlje Commune";
        case 964:
            return "Trebnje Commune";
        case 965:
            return "Trzic Commune";
        case 966:
            return "Turnisce Commune";
        case 967:
            return "Velenje Urban Commune";
        case 968:
            return "Velike Lasce Commune";
        case 1004:
            return "Vipava Commune";
        case 1005:
            return "Vitanje Commune";
        case 1006:
            return "Vodice Commune";
        case 1008:
            return "Vrhnika Commune";
        case 1009:
            return "Vuzenica Commune";
        case 1010:
            return "Zagorje ob Savi Commune";
        case 1012:
            return "Zavrc Commune";
        case 1047:
            return "Zelezniki Commune";
        case 1048:
            return "Ziri Commune";
        case 1049:
            return "Zrece Commune";
        case 1050:
            return "Benedikt Commune";
        case 1051:
            return "Bistrica ob Sotli Commune";
        case 1052:
            return "Bloke Commune";
        case 1053:
            return "Braslovce Commune";
        case 1054:
            return "Cankova Commune";
        case 1055:
            return "Cerkvenjak Commune";
        case 1090:
            return "Destrnik Commune";
        case 1091:
            return "Dobje Commune";
        case 1092:
            return "Dobrna Commune";
        case 1093:
            return "Dobrova-Horjul-Polhov Gradec Commune";
        case 1094:
            return "Dobrovnik-Dobronak Commune";
        case 1095:
            return "Dolenjske Toplice Commune";
        case 1096:
            return "Domzale Commune";
        case 1097:
            return "Grad Commune";
        case 1098:
            return "Hajdina Commune";
        case 1133:
            return "Hoce-Slivnica Commune";
        case 1134:
            return "Hodos-Hodos Commune";
        case 1135:
            return "Horjul Commune";
        case 1136:
            return "Jesenice Commune";
        case 1137:
            return "Jezersko Commune";
        case 1138:
            return "Kamnik Commune";
        case 1139:
            return "Kocevje Commune";
        case 1140:
            return "Komenda Commune";
        case 1141:
            return "Kostel Commune";
        case 1176:
            return "Krizevci Commune";
        case 1177:
            return "Kuzma Commune";
        case 1178:
            return "Lenart Commune";
        case 1179:
            return "Lendava-Lendva Commune";
        case 1180:
            return "Litija Commune";
        case 1181:
            return "Ljutomer Commune";
        case 1182:
            return "Loska Dolina Commune";
        case 1183:
            return "Lovrenc na Pohorju Commune";
        case 1184:
            return "Luce Commune";
        case 1219:
            return "Majsperk Commune";
        case 1220:
            return "Maribor Commune";
        case 1221:
            return "Markovci Commune";
        case 1222:
            return "Miklavz na Dravskem polju Commune";
        case 1223:
            return "Miren-Kostanjevica Commune";
        case 1224:
            return "Mirna Pec Commune";
        case 1225:
            return "Novo mesto Urban Commune";
        case 1226:
            return "Oplotnica Commune";
        case 1227:
            return "Piran-Pirano Commune";
        case 1262:
            return "Podlehnik Commune";
        case 1263:
            return "Podvelka Commune";
        case 1264:
            return "Polzela Commune";
        case 1265:
            return "Prebold Commune";
        case 1266:
            return "Preddvor Commune";
        case 1267:
            return "Prevalje Commune";
        case 1268:
            return "Ptuj Urban Commune";
        case 1269:
            return "Ravne na Koroskem Commune";
        case 1270:
            return "Razkrizje Commune";
        case 1305:
            return "Ribnica Commune";
        case 1306:
            return "Ribnica na Pohorju Commune";
        case 1307:
            return "Ruse Commune";
        case 1308:
            return "Salovci Commune";
        case 1309:
            return "Selnica ob Dravi Commune";
        case 1310:
            return "Sempeter-Vrtojba Commune";
        case 1311:
            return "Sentjur pri Celju Commune";
        case 1312:
            return "Slovenska Bistrica Commune";
        case 1313:
            return "Smartno pri Litiji Commune";
        case 1348:
            return "Sodrazica Commune";
        case 1349:
            return "Solcava Commune";
        case 1350:
            return "Sveta Ana Commune";
        case 1351:
            return "Sveti Andraz v Slovenskih goricah Commune";
        case 1352:
            return "Tabor Commune";
        case 1353:
            return "Tisina Commune";
        case 1354:
            return "Trnovska vas Commune";
        case 1355:
            return "Trzin Commune";
        case 1356:
            return "Velika Polana Commune";
        case 1391:
            return "Verzej Commune";
        case 1392:
            return "Videm Commune";
        case 1393:
            return "Vojnik Commune";
        case 1394:
            return "Vransko Commune";
        case 1395:
            return "Zalec Commune";
        case 1396:
            return "Zetale Commune";
        case 1397:
            return "Zirovnica Commune";
        case 1398:
            return "Zuzemberk Commune";
        case 1399:
            return "Apace Commune";
        case 1434:
            return "Cirkulane Commune";
        case 1435:
            return "Gorje";
        case 1436:
            return "Kostanjevica na Krki";
        case 1437:
            return "Log-Dragomer";
        case 1438:
            return "Makole";
        case 1439:
            return "Mirna";
        case 1440:
            return "Mokronog-Trebelno";
        case 1441:
            return "Poljcane";
        case 1442:
            return "Recica ob Savinji";
        case 1477:
            return "Rence-Vogrsko";
        case 1478:
            return "Sentrupert";
        case 1479:
            return "Smarjesk Toplice";
        case 1480:
            return "Sredisce ob Dravi";
        case 1481:
            return "Straza";
        case 1483:
            return "Sveti Jurij v Slovenskih Goricah";
        }
    }else if (strcmp(country_code, "SK") == 0) {
        switch (region_code2) {
        case 1:
            return "Banska Bystrica";
        case 2:
            return "Bratislava";
        case 3:
            return "Kosice";
        case 4:
            return "Nitra";
        case 5:
            return "Presov";
        case 6:
            return "Trencin";
        case 7:
            return "Trnava";
        case 8:
            return "Zilina";
        }
    }else if (strcmp(country_code, "SL") == 0) {
        switch (region_code2) {
        case 1:
            return "Eastern";
        case 2:
            return "Northern";
        case 3:
            return "Southern";
        case 4:
            return "Western Area";
        }
    }else if (strcmp(country_code, "SM") == 0) {
        switch (region_code2) {
        case 1:
            return "Acquaviva";
        case 2:
            return "Chiesanuova";
        case 3:
            return "Domagnano";
        case 4:
            return "Faetano";
        case 5:
            return "Fiorentino";
        case 6:
            return "Borgo Maggiore";
        case 7:
            return "San Marino";
        case 8:
            return "Monte Giardino";
        case 9:
            return "Serravalle";
        }
    }else if (strcmp(country_code, "SN") == 0) {
        switch (region_code2) {
        case 1:
            return "Dakar";
        case 3:
            return "Diourbel";
        case 5:
            return "Tambacounda";
        case 7:
            return "Thies";
        case 9:
            return "Fatick";
        case 10:
            return "Kaolack";
        case 11:
            return "Kolda";
        case 12:
            return "Ziguinchor";
        case 13:
            return "Louga";
        case 14:
            return "Saint-Louis";
        case 15:
            return "Matam";
        }
    }else if (strcmp(country_code, "SO") == 0) {
        switch (region_code2) {
        case 1:
            return "Bakool";
        case 2:
            return "Banaadir";
        case 3:
            return "Bari";
        case 4:
            return "Bay";
        case 5:
            return "Galguduud";
        case 6:
            return "Gedo";
        case 7:
            return "Hiiraan";
        case 8:
            return "Jubbada Dhexe";
        case 9:
            return "Jubbada Hoose";
        case 10:
            return "Mudug";
        case 11:
            return "Nugaal";
        case 12:
            return "Sanaag";
        case 13:
            return "Shabeellaha Dhexe";
        case 14:
            return "Shabeellaha Hoose";
        case 16:
            return "Woqooyi Galbeed";
        case 18:
            return "Nugaal";
        case 19:
            return "Togdheer";
        case 20:
            return "Woqooyi Galbeed";
        case 21:
            return "Awdal";
        case 22:
            return "Sool";
        }
    }else if (strcmp(country_code, "SR") == 0) {
        switch (region_code2) {
        case 10:
            return "Brokopondo";
        case 11:
            return "Commewijne";
        case 12:
            return "Coronie";
        case 13:
            return "Marowijne";
        case 14:
            return "Nickerie";
        case 15:
            return "Para";
        case 16:
            return "Paramaribo";
        case 17:
            return "Saramacca";
        case 18:
            return "Sipaliwini";
        case 19:
            return "Wanica";
        }
    }else if (strcmp(country_code, "SS") == 0) {
        switch (region_code2) {
        case 1:
            return "Central Equatoria";
        case 2:
            return "Eastern Equatoria";
        case 3:
            return "Jonglei";
        case 4:
            return "Lakes";
        case 5:
            return "Northern Bahr el Ghazal";
        case 6:
            return "Unity";
        case 7:
            return "Upper Nile";
        case 8:
            return "Warrap";
        case 9:
            return "Western Bahr el Ghazal";
        case 10:
            return "Western Equatoria";
        }
    }else if (strcmp(country_code, "ST") == 0) {
        switch (region_code2) {
        case 1:
            return "Principe";
        case 2:
            return "Sao Tome";
        }
    }else if (strcmp(country_code, "SV") == 0) {
        switch (region_code2) {
        case 1:
            return "Ahuachapan";
        case 2:
            return "Cabanas";
        case 3:
            return "Chalatenango";
        case 4:
            return "Cuscatlan";
        case 5:
            return "La Libertad";
        case 6:
            return "La Paz";
        case 7:
            return "La Union";
        case 8:
            return "Morazan";
        case 9:
            return "San Miguel";
        case 10:
            return "San Salvador";
        case 11:
            return "Santa Ana";
        case 12:
            return "San Vicente";
        case 13:
            return "Sonsonate";
        case 14:
            return "Usulutan";
        }
    }else if (strcmp(country_code, "SY") == 0) {
        switch (region_code2) {
        case 1:
            return "Al Hasakah";
        case 2:
            return "Al Ladhiqiyah";
        case 3:
            return "Al Qunaytirah";
        case 4:
            return "Ar Raqqah";
        case 5:
            return "As Suwayda'";
        case 6:
            return "Dar";
        case 7:
            return "Dayr az Zawr";
        case 8:
            return "Rif Dimashq";
        case 9:
            return "Halab";
        case 10:
            return "Hamah";
        case 11:
            return "Hims";
        case 12:
            return "Idlib";
        case 13:
            return "Dimashq";
        case 14:
            return "Tartus";
        }
    }else if (strcmp(country_code, "SZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Hhohho";
        case 2:
            return "Lubombo";
        case 3:
            return "Manzini";
        case 4:
            return "Shiselweni";
        case 5:
            return "Praslin";
        }
    }else if (strcmp(country_code, "TD") == 0) {
        switch (region_code2) {
        case 1:
            return "Batha";
        case 2:
            return "Biltine";
        case 3:
            return "Borkou-Ennedi-Tibesti";
        case 4:
            return "Chari-Baguirmi";
        case 5:
            return "Guera";
        case 6:
            return "Kanem";
        case 7:
            return "Lac";
        case 8:
            return "Logone Occidental";
        case 9:
            return "Logone Oriental";
        case 10:
            return "Mayo-Kebbi";
        case 11:
            return "Moyen-Chari";
        case 12:
            return "Ouaddai";
        case 13:
            return "Salamat";
        case 14:
            return "Tandjile";
        }
    }else if (strcmp(country_code, "TG") == 0) {
        switch (region_code2) {
        case 22:
            return "Centrale";
        case 23:
            return "Kara";
        case 24:
            return "Maritime";
        case 25:
            return "Plateaux";
        case 26:
            return "Savanes";
        }
    }else if (strcmp(country_code, "TH") == 0) {
        switch (region_code2) {
        case 1:
            return "Mae Hong Son";
        case 2:
            return "Chiang Mai";
        case 3:
            return "Chiang Rai";
        case 4:
            return "Nan";
        case 5:
            return "Lamphun";
        case 6:
            return "Lampang";
        case 7:
            return "Phrae";
        case 8:
            return "Tak";
        case 9:
            return "Sukhothai";
        case 10:
            return "Uttaradit";
        case 11:
            return "Kamphaeng Phet";
        case 12:
            return "Phitsanulok";
        case 13:
            return "Phichit";
        case 14:
            return "Phetchabun";
        case 15:
            return "Uthai Thani";
        case 16:
            return "Nakhon Sawan";
        case 17:
            return "Nong Khai";
        case 18:
            return "Loei";
        case 20:
            return "Sakon Nakhon";
        case 21:
            return "Nakhon Phanom";
        case 22:
            return "Khon Kaen";
        case 23:
            return "Kalasin";
        case 24:
            return "Maha Sarakham";
        case 25:
            return "Roi Et";
        case 26:
            return "Chaiyaphum";
        case 27:
            return "Nakhon Ratchasima";
        case 28:
            return "Buriram";
        case 29:
            return "Surin";
        case 30:
            return "Sisaket";
        case 31:
            return "Narathiwat";
        case 32:
            return "Chai Nat";
        case 33:
            return "Sing Buri";
        case 34:
            return "Lop Buri";
        case 35:
            return "Ang Thong";
        case 36:
            return "Phra Nakhon Si Ayutthaya";
        case 37:
            return "Saraburi";
        case 38:
            return "Nonthaburi";
        case 39:
            return "Pathum Thani";
        case 40:
            return "Krung Thep";
        case 41:
            return "Phayao";
        case 42:
            return "Samut Prakan";
        case 43:
            return "Nakhon Nayok";
        case 44:
            return "Chachoengsao";
        case 45:
            return "Prachin Buri";
        case 46:
            return "Chon Buri";
        case 47:
            return "Rayong";
        case 48:
            return "Chanthaburi";
        case 49:
            return "Trat";
        case 50:
            return "Kanchanaburi";
        case 51:
            return "Suphan Buri";
        case 52:
            return "Ratchaburi";
        case 53:
            return "Nakhon Pathom";
        case 54:
            return "Samut Songkhram";
        case 55:
            return "Samut Sakhon";
        case 56:
            return "Phetchaburi";
        case 57:
            return "Prachuap Khiri Khan";
        case 58:
            return "Chumphon";
        case 59:
            return "Ranong";
        case 60:
            return "Surat Thani";
        case 61:
            return "Phangnga";
        case 62:
            return "Phuket";
        case 63:
            return "Krabi";
        case 64:
            return "Nakhon Si Thammarat";
        case 65:
            return "Trang";
        case 66:
            return "Phatthalung";
        case 67:
            return "Satun";
        case 68:
            return "Songkhla";
        case 69:
            return "Pattani";
        case 70:
            return "Yala";
        case 71:
            return "Ubon Ratchathani";
        case 72:
            return "Yasothon";
        case 73:
            return "Nakhon Phanom";
        case 74:
            return "Prachin Buri";
        case 75:
            return "Ubon Ratchathani";
        case 76:
            return "Udon Thani";
        case 77:
            return "Amnat Charoen";
        case 78:
            return "Mukdahan";
        case 79:
            return "Nong Bua Lamphu";
        case 80:
            return "Sa Kaeo";
        case 81:
            return "Bueng Kan";
        }
    }else if (strcmp(country_code, "TJ") == 0) {
        switch (region_code2) {
        case 1:
            return "Kuhistoni Badakhshon";
        case 2:
            return "Khatlon";
        case 3:
            return "Sughd";
        case 4:
            return "Dushanbe";
        case 5:
            return "Nohiyahoi Tobei Jumhuri";
        }
    }else if (strcmp(country_code, "TL") == 0) {
        switch (region_code2) {
        case 6:
            return "Dili";
        }
    }else if (strcmp(country_code, "TM") == 0) {
        switch (region_code2) {
        case 1:
            return "Ahal";
        case 2:
            return "Balkan";
        case 3:
            return "Dashoguz";
        case 4:
            return "Lebap";
        case 5:
            return "Mary";
        }
    }else if (strcmp(country_code, "TN") == 0) {
        switch (region_code2) {
        case 2:
            return "Kasserine";
        case 3:
            return "Kairouan";
        case 6:
            return "Jendouba";
        case 10:
            return "Qafsah";
        case 14:
            return "El Kef";
        case 15:
            return "Al Mahdia";
        case 16:
            return "Al Munastir";
        case 17:
            return "Bajah";
        case 18:
            return "Bizerte";
        case 19:
            return "Nabeul";
        case 22:
            return "Siliana";
        case 23:
            return "Sousse";
        case 27:
            return "Ben Arous";
        case 28:
            return "Madanin";
        case 29:
            return "Gabes";
        case 31:
            return "Kebili";
        case 32:
            return "Sfax";
        case 33:
            return "Sidi Bou Zid";
        case 34:
            return "Tataouine";
        case 35:
            return "Tozeur";
        case 36:
            return "Tunis";
        case 37:
            return "Zaghouan";
        case 38:
            return "Aiana";
        case 39:
            return "Manouba";
        }
    }else if (strcmp(country_code, "TO") == 0) {
        switch (region_code2) {
        case 1:
            return "Ha";
        case 2:
            return "Tongatapu";
        case 3:
            return "Vava";
        }
    }else if (strcmp(country_code, "TR") == 0) {
        switch (region_code2) {
        case 2:
            return "Adiyaman";
        case 3:
            return "Afyonkarahisar";
        case 4:
            return "Agri";
        case 5:
            return "Amasya";
        case 7:
            return "Antalya";
        case 8:
            return "Artvin";
        case 9:
            return "Aydin";
        case 10:
            return "Balikesir";
        case 11:
            return "Bilecik";
        case 12:
            return "Bingol";
        case 13:
            return "Bitlis";
        case 14:
            return "Bolu";
        case 15:
            return "Burdur";
        case 16:
            return "Bursa";
        case 17:
            return "Canakkale";
        case 19:
            return "Corum";
        case 20:
            return "Denizli";
        case 21:
            return "Diyarbakir";
        case 22:
            return "Edirne";
        case 23:
            return "Elazig";
        case 24:
            return "Erzincan";
        case 25:
            return "Erzurum";
        case 26:
            return "Eskisehir";
        case 28:
            return "Giresun";
        case 31:
            return "Hatay";
        case 32:
            return "Mersin";
        case 33:
            return "Isparta";
        case 34:
            return "Istanbul";
        case 35:
            return "Izmir";
        case 37:
            return "Kastamonu";
        case 38:
            return "Kayseri";
        case 39:
            return "Kirklareli";
        case 40:
            return "Kirsehir";
        case 41:
            return "Kocaeli";
        case 43:
            return "Kutahya";
        case 44:
            return "Malatya";
        case 45:
            return "Manisa";
        case 46:
            return "Kahramanmaras";
        case 48:
            return "Mugla";
        case 49:
            return "Mus";
        case 50:
            return "Nevsehir";
        case 52:
            return "Ordu";
        case 53:
            return "Rize";
        case 54:
            return "Sakarya";
        case 55:
            return "Samsun";
        case 57:
            return "Sinop";
        case 58:
            return "Sivas";
        case 59:
            return "Tekirdag";
        case 60:
            return "Tokat";
        case 61:
            return "Trabzon";
        case 62:
            return "Tunceli";
        case 63:
            return "Sanliurfa";
        case 64:
            return "Usak";
        case 65:
            return "Van";
        case 66:
            return "Yozgat";
        case 68:
            return "Ankara";
        case 69:
            return "Gumushane";
        case 70:
            return "Hakkari";
        case 71:
            return "Konya";
        case 72:
            return "Mardin";
        case 73:
            return "Nigde";
        case 74:
            return "Siirt";
        case 75:
            return "Aksaray";
        case 76:
            return "Batman";
        case 77:
            return "Bayburt";
        case 78:
            return "Karaman";
        case 79:
            return "Kirikkale";
        case 80:
            return "Sirnak";
        case 81:
            return "Adana";
        case 82:
            return "Cankiri";
        case 83:
            return "Gaziantep";
        case 84:
            return "Kars";
        case 85:
            return "Zonguldak";
        case 86:
            return "Ardahan";
        case 87:
            return "Bartin";
        case 88:
            return "Igdir";
        case 89:
            return "Karabuk";
        case 90:
            return "Kilis";
        case 91:
            return "Osmaniye";
        case 92:
            return "Yalova";
        case 93:
            return "Duzce";
        }
    }else if (strcmp(country_code, "TT") == 0) {
        switch (region_code2) {
        case 1:
            return "Arima";
        case 2:
            return "Caroni";
        case 3:
            return "Mayaro";
        case 4:
            return "Nariva";
        case 5:
            return "Port-of-Spain";
        case 6:
            return "Saint Andrew";
        case 7:
            return "Saint David";
        case 8:
            return "Saint George";
        case 9:
            return "Saint Patrick";
        case 10:
            return "San Fernando";
        case 11:
            return "Tobago";
        case 12:
            return "Victoria";
        }
    }else if (strcmp(country_code, "TW") == 0) {
        switch (region_code2) {
        case 1:
            return "Fu-chien";
        case 2:
            return "Kao-hsiung";
        case 3:
            return "T'ai-pei";
        case 4:
            return "T'ai-wan";
        }
    }else if (strcmp(country_code, "TZ") == 0) {
        switch (region_code2) {
        case 2:
            return "Pwani";
        case 3:
            return "Dodoma";
        case 4:
            return "Iringa";
        case 5:
            return "Kigoma";
        case 6:
            return "Kilimanjaro";
        case 7:
            return "Lindi";
        case 8:
            return "Mara";
        case 9:
            return "Mbeya";
        case 10:
            return "Morogoro";
        case 11:
            return "Mtwara";
        case 12:
            return "Mwanza";
        case 13:
            return "Pemba North";
        case 14:
            return "Ruvuma";
        case 15:
            return "Shinyanga";
        case 16:
            return "Singida";
        case 17:
            return "Tabora";
        case 18:
            return "Tanga";
        case 19:
            return "Kagera";
        case 20:
            return "Pemba South";
        case 21:
            return "Zanzibar Central";
        case 22:
            return "Zanzibar North";
        case 23:
            return "Dar es Salaam";
        case 24:
            return "Rukwa";
        case 25:
            return "Zanzibar Urban";
        case 26:
            return "Arusha";
        case 27:
            return "Manyara";
        }
    }else if (strcmp(country_code, "UA") == 0) {
        switch (region_code2) {
        case 1:
            return "Cherkas'ka Oblast'";
        case 2:
            return "Chernihivs'ka Oblast'";
        case 3:
            return "Chernivets'ka Oblast'";
        case 4:
            return "Dnipropetrovs'ka Oblast'";
        case 5:
            return "Donets'ka Oblast'";
        case 6:
            return "Ivano-Frankivs'ka Oblast'";
        case 7:
            return "Kharkivs'ka Oblast'";
        case 8:
            return "Khersons'ka Oblast'";
        case 9:
            return "Khmel'nyts'ka Oblast'";
        case 10:
            return "Kirovohrads'ka Oblast'";
        case 11:
            return "Krym";
        case 12:
            return "Kyyiv";
        case 13:
            return "Kyyivs'ka Oblast'";
        case 14:
            return "Luhans'ka Oblast'";
        case 15:
            return "L'vivs'ka Oblast'";
        case 16:
            return "Mykolayivs'ka Oblast'";
        case 17:
            return "Odes'ka Oblast'";
        case 18:
            return "Poltavs'ka Oblast'";
        case 19:
            return "Rivnens'ka Oblast'";
        case 20:
            return "Sevastopol'";
        case 21:
            return "Sums'ka Oblast'";
        case 22:
            return "Ternopil's'ka Oblast'";
        case 23:
            return "Vinnyts'ka Oblast'";
        case 24:
            return "Volyns'ka Oblast'";
        case 25:
            return "Zakarpats'ka Oblast'";
        case 26:
            return "Zaporiz'ka Oblast'";
        case 27:
            return "Zhytomyrs'ka Oblast'";
        }
    }else if (strcmp(country_code, "UG") == 0) {
        switch (region_code2) {
        case 26:
            return "Apac";
        case 28:
            return "Bundibugyo";
        case 29:
            return "Bushenyi";
        case 30:
            return "Gulu";
        case 31:
            return "Hoima";
        case 33:
            return "Jinja";
        case 36:
            return "Kalangala";
        case 37:
            return "Kampala";
        case 38:
            return "Kamuli";
        case 39:
            return "Kapchorwa";
        case 40:
            return "Kasese";
        case 41:
            return "Kibale";
        case 42:
            return "Kiboga";
        case 43:
            return "Kisoro";
        case 45:
            return "Kotido";
        case 46:
            return "Kumi";
        case 47:
            return "Lira";
        case 50:
            return "Masindi";
        case 52:
            return "Mbarara";
        case 56:
            return "Mubende";
        case 58:
            return "Nebbi";
        case 59:
            return "Ntungamo";
        case 60:
            return "Pallisa";
        case 61:
            return "Rakai";
        case 65:
            return "Adjumani";
        case 66:
            return "Bugiri";
        case 67:
            return "Busia";
        case 69:
            return "Katakwi";
        case 70:
            return "Luwero";
        case 71:
            return "Masaka";
        case 72:
            return "Moyo";
        case 73:
            return "Nakasongola";
        case 74:
            return "Sembabule";
        case 76:
            return "Tororo";
        case 77:
            return "Arua";
        case 78:
            return "Iganga";
        case 79:
            return "Kabarole";
        case 80:
            return "Kaberamaido";
        case 81:
            return "Kamwenge";
        case 82:
            return "Kanungu";
        case 83:
            return "Kayunga";
        case 84:
            return "Kitgum";
        case 85:
            return "Kyenjojo";
        case 86:
            return "Mayuge";
        case 87:
            return "Mbale";
        case 88:
            return "Moroto";
        case 89:
            return "Mpigi";
        case 90:
            return "Mukono";
        case 91:
            return "Nakapiripirit";
        case 92:
            return "Pader";
        case 93:
            return "Rukungiri";
        case 94:
            return "Sironko";
        case 95:
            return "Soroti";
        case 96:
            return "Wakiso";
        case 97:
            return "Yumbe";
        }
    }else if (strcmp(country_code, "UY") == 0) {
        switch (region_code2) {
        case 1:
            return "Artigas";
        case 2:
            return "Canelones";
        case 3:
            return "Cerro Largo";
        case 4:
            return "Colonia";
        case 5:
            return "Durazno";
        case 6:
            return "Flores";
        case 7:
            return "Florida";
        case 8:
            return "Lavalleja";
        case 9:
            return "Maldonado";
        case 10:
            return "Montevideo";
        case 11:
            return "Paysandu";
        case 12:
            return "Rio Negro";
        case 13:
            return "Rivera";
        case 14:
            return "Rocha";
        case 15:
            return "Salto";
        case 16:
            return "San Jose";
        case 17:
            return "Soriano";
        case 18:
            return "Tacuarembo";
        case 19:
            return "Treinta y Tres";
        }
    }else if (strcmp(country_code, "UZ") == 0) {
        switch (region_code2) {
        case 1:
            return "Andijon";
        case 2:
            return "Bukhoro";
        case 3:
            return "Farghona";
        case 4:
            return "Jizzakh";
        case 5:
            return "Khorazm";
        case 6:
            return "Namangan";
        case 7:
            return "Nawoiy";
        case 8:
            return "Qashqadaryo";
        case 9:
            return "Qoraqalpoghiston";
        case 10:
            return "Samarqand";
        case 11:
            return "Sirdaryo";
        case 12:
            return "Surkhondaryo";
        case 13:
            return "Toshkent";
        case 14:
            return "Toshkent";
        case 15:
            return "Jizzax";
        }
    }else if (strcmp(country_code, "VC") == 0) {
        switch (region_code2) {
        case 1:
            return "Charlotte";
        case 2:
            return "Saint Andrew";
        case 3:
            return "Saint David";
        case 4:
            return "Saint George";
        case 5:
            return "Saint Patrick";
        case 6:
            return "Grenadines";
        }
    }else if (strcmp(country_code, "VE") == 0) {
        switch (region_code2) {
        case 1:
            return "Amazonas";
        case 2:
            return "Anzoategui";
        case 3:
            return "Apure";
        case 4:
            return "Aragua";
        case 5:
            return "Barinas";
        case 6:
            return "Bolivar";
        case 7:
            return "Carabobo";
        case 8:
            return "Cojedes";
        case 9:
            return "Delta Amacuro";
        case 11:
            return "Falcon";
        case 12:
            return "Guarico";
        case 13:
            return "Lara";
        case 14:
            return "Merida";
        case 15:
            return "Miranda";
        case 16:
            return "Monagas";
        case 17:
            return "Nueva Esparta";
        case 18:
            return "Portuguesa";
        case 19:
            return "Sucre";
        case 20:
            return "Tachira";
        case 21:
            return "Trujillo";
        case 22:
            return "Yaracuy";
        case 23:
            return "Zulia";
        case 24:
            return "Dependencias Federales";
        case 25:
            return "Distrito Federal";
        case 26:
            return "Vargas";
        }
    }else if (strcmp(country_code, "VN") == 0) {
        switch (region_code2) {
        case 1:
            return "An Giang";
        case 3:
            return "Ben Tre";
        case 5:
            return "Cao Bang";
        case 9:
            return "Dong Thap";
        case 13:
            return "Hai Phong";
        case 20:
            return "Ho Chi Minh";
        case 21:
            return "Kien Giang";
        case 23:
            return "Lam Dong";
        case 24:
            return "Long An";
        case 30:
            return "Quang Ninh";
        case 32:
            return "Son La";
        case 33:
            return "Tay Ninh";
        case 34:
            return "Thanh Hoa";
        case 35:
            return "Thai Binh";
        case 37:
            return "Tien Giang";
        case 39:
            return "Lang Son";
        case 43:
            return "Dong Nai";
        case 44:
            return "Ha Noi";
        case 45:
            return "Ba Ria-Vung Tau";
        case 46:
            return "Binh Dinh";
        case 47:
            return "Binh Thuan";
        case 49:
            return "Gia Lai";
        case 50:
            return "Ha Giang";
        case 52:
            return "Ha Tinh";
        case 53:
            return "Hoa Binh";
        case 54:
            return "Khanh Hoa";
        case 55:
            return "Kon Tum";
        case 58:
            return "Nghe An";
        case 59:
            return "Ninh Binh";
        case 60:
            return "Ninh Thuan";
        case 61:
            return "Phu Yen";
        case 62:
            return "Quang Binh";
        case 63:
            return "Quang Ngai";
        case 64:
            return "Quang Tri";
        case 65:
            return "Soc Trang";
        case 66:
            return "Thua Thien-Hue";
        case 67:
            return "Tra Vinh";
        case 68:
            return "Tuyen Quang";
        case 69:
            return "Vinh Long";
        case 70:
            return "Yen Bai";
        case 71:
            return "Bac Giang";
        case 72:
            return "Bac Kan";
        case 73:
            return "Bac Lieu";
        case 74:
            return "Bac Ninh";
        case 75:
            return "Binh Duong";
        case 76:
            return "Binh Phuoc";
        case 77:
            return "Ca Mau";
        case 78:
            return "Da Nang";
        case 79:
            return "Hai Duong";
        case 80:
            return "Ha Nam";
        case 81:
            return "Hung Yen";
        case 82:
            return "Nam Dinh";
        case 83:
            return "Phu Tho";
        case 84:
            return "Quang Nam";
        case 85:
            return "Thai Nguyen";
        case 86:
            return "Vinh Phuc";
        case 87:
            return "Can Tho";
        case 88:
            return "Dac Lak";
        case 89:
            return "Lai Chau";
        case 90:
            return "Lao Cai";
        case 91:
            return "Dak Nong";
        case 92:
            return "Dien Bien";
        case 93:
            return "Hau Giang";
        }
    }else if (strcmp(country_code, "VU") == 0) {
        switch (region_code2) {
        case 5:
            return "Ambrym";
        case 6:
            return "Aoba";
        case 7:
            return "Torba";
        case 8:
            return "Efate";
        case 9:
            return "Epi";
        case 10:
            return "Malakula";
        case 11:
            return "Paama";
        case 12:
            return "Pentecote";
        case 13:
            return "Sanma";
        case 14:
            return "Shepherd";
        case 15:
            return "Tafea";
        case 16:
            return "Malampa";
        case 17:
            return "Penama";
        case 18:
            return "Shefa";
        }
    }else if (strcmp(country_code, "WS") == 0) {
        switch (region_code2) {
        case 2:
            return "Aiga-i-le-Tai";
        case 3:
            return "Atua";
        case 4:
            return "Fa";
        case 5:
            return "Gaga";
        case 6:
            return "Va";
        case 7:
            return "Gagaifomauga";
        case 8:
            return "Palauli";
        case 9:
            return "Satupa";
        case 10:
            return "Tuamasaga";
        case 11:
            return "Vaisigano";
        }
    }else if (strcmp(country_code, "YE") == 0) {
        switch (region_code2) {
        case 1:
            return "Abyan";
        case 2:
            return "Adan";
        case 3:
            return "Al Mahrah";
        case 4:
            return "Hadramawt";
        case 5:
            return "Shabwah";
        case 6:
            return "Lahij";
        case 7:
            return "Al Bayda'";
        case 8:
            return "Al Hudaydah";
        case 9:
            return "Al Jawf";
        case 10:
            return "Al Mahwit";
        case 11:
            return "Dhamar";
        case 12:
            return "Hajjah";
        case 13:
            return "Ibb";
        case 14:
            return "Ma'rib";
        case 15:
            return "Sa'dah";
        case 16:
            return "San'a'";
        case 17:
            return "Taizz";
        case 18:
            return "Ad Dali";
        case 19:
            return "Amran";
        case 20:
            return "Al Bayda'";
        case 21:
            return "Al Jawf";
        case 22:
            return "Hajjah";
        case 23:
            return "Ibb";
        case 24:
            return "Lahij";
        case 25:
            return "Taizz";
        }
    }else if (strcmp(country_code, "ZA") == 0) {
        switch (region_code2) {
        case 1:
            return "North-Western Province";
        case 2:
            return "KwaZulu-Natal";
        case 3:
            return "Free State";
        case 5:
            return "Eastern Cape";
        case 6:
            return "Gauteng";
        case 7:
            return "Mpumalanga";
        case 8:
            return "Northern Cape";
        case 9:
            return "Limpopo";
        case 10:
            return "North-West";
        case 11:
            return "Western Cape";
        }
    }else if (strcmp(country_code, "ZM") == 0) {
        switch (region_code2) {
        case 1:
            return "Western";
        case 2:
            return "Central";
        case 3:
            return "Eastern";
        case 4:
            return "Luapula";
        case 5:
            return "Northern";
        case 6:
            return "North-Western";
        case 7:
            return "Southern";
        case 8:
            return "Copperbelt";
        case 9:
            return "Lusaka";
        }
    }else if (strcmp(country_code, "ZW") == 0) {
        switch (region_code2) {
        case 1:
            return "Manicaland";
        case 2:
            return "Midlands";
        case 3:
            return "Mashonaland Central";
        case 4:
            return "Mashonaland East";
        case 5:
            return "Mashonaland West";
        case 6:
            return "Matabeleland North";
        case 7:
            return "Matabeleland South";
        case 8:
            return "Masvingo";
        case 9:
            return "Bulawayo";
        case 10:
            return "Harare";
        }
    }
    return name;
}
