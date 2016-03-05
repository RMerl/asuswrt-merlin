/* @(#) HTML-page templates/generation for udpxy status page
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STATPG_H_0110081157_
#define STATPG_H_0110081157_

#ifdef __cplusplus
    extern "C" {
#endif

/* NB: strings are exposed in the header to be included
 *     by unit tests, yet made static to confine to a single unit */

static const char HTML_PAGE_HEADER[] =
        "HTTP/1.0 200 OK\n"
        "Content-type: text/html\n\n";

static const char* STAT_PAGE_BASE[] = {
    "<html>\n"
    "<head>\n"
    "<style type=\"text/css\">\n"
    "h1 {\n"
    "    margin: 0;\n"
    "    padding: 1em 0.5em 0.5em;\n"
    "    font-size: 120%;\n"
    "    letter-spacing:0.1em;\n"
    "    font-family: serif; \n"
    "}\n"
    "h3{\n"
    "    margin: 1em 0 0;\n"
    "    font-weight:bold;\n"
    "    font-size: 110%;\n"
    "    letter-spacing:0.1em;\n"
    "}\n"
    "table {\n"
    "    margin:0.5em auto 0.2em auto;\n"
    "    padding:0;\n"
    "    background:#fff;\n"
    "    font-size: 100%;\n"
    "    border-collapse:collapse;\n"
    "}\n"
    "th {\n"
    "    font-weight:bold;\n"
    "    font-size: 110%;\n"
    "    border: 1px solid #333;\n"
    "    padding: 0.5em 0.5em 0.2em;\n"
    "}\n",

    "td {\n"
    "    border: 1px solid #333;\n"
    "    padding: 0.2em 0.5em;\n"
    "}\n"
    ".proctabl th {\n"
    "    background: #FFF;\n"
    "    color:#000;\n"
    "}\n"
    ".proctabl tr.uneven{\n"
    "    background: #dde;\n"
    "}\n"
    "#bodyCon{\n"
    "    margin:0;\n"
    "    padding:0;\n"
    "    border: 1px solid #334;\n"
    "    width: 750px;\n"
    "    font: 90% Arial, sans-serif;\n"
    "    text-align:center;\n"
    "}\n",

    "#pgCont {\n"
    "    margin: 1em;\n"
    "    padding: 2em 2em 0.5em;\n"
    "}\n"
    "#footer {\n"
    "    margin: 2em 0 0.5em;\n"
    "    font-size: 80%;\n"
    "    color:#555;\n"
    "    letter-spacing:0.1em;\n"
    "    font-family: Verdana,Arial,sans-serif;\n"
    "}\n"
    "</style>\n"
    "</head>\n"
};
static const size_t LEN_STAT_PAGE_BASE = 3;

static const char STAT_PAGE_FMT1[] =
    "<body>\n"
    "<div id=\"bodyCon\">\n"
    "<h1>udpxy status:</h1>\n"
    "<div id=\"pgCont\">\n"
    "<table cellspacing=\"0\">\n"
    "<tr>\n"
    "    <th>Server Process ID</th>\n"
    "    <th>Accepting clients on</th>\n"
    "    <th>Multicast address</th>\n"
    "    <th>Active clients</th>\n"
    "</tr>\n"
    "<tr>\n"
    "    <td>%d</td>\n"
    "    <td>%s:%d</td>\n"
    "    <td>%s</td>\n"
    "    <td>%d</td>\n"
    "</tr>\n"
    "</table>\n"
    "<form action=\"/restart/\" method=\"get\">\n"
    "<input type=\"submit\" value=\"Restart\">\n"
    "</form>\n"
    "\n"
    "%s"  /* Active clients table goes here */
    "%s"  /* Usage guide table */
    "</div>\n"
    "<div id=\"footer\">udpxy v. %s (Build %d) %s - [%s]<br>%s</div>\n"     /* footer */
    "</div>\n"
    "</body>\n"
    "</html>\n";


static const char RST_PAGE_FMT1[] =
    "<body onload=\"body_onload();\">\n"
    "<div id=\"bodyCon\">\n"
    "<h1>udpxy is RESTARTING - This page will refresh automatically</h1>\n"
    "<div id=\"pgCont\">\n"
    "<table cellspacing=\"0\">\n"
    "<tr>\n"
    "    <th>Server Process ID</th>\n"
    "    <th>Accepting clients on</th>\n"
    "    <th>Multicast address</th>\n"
    "    <th>Active clients</th>\n"
    "</tr>\n"
    "<tr>\n"
    "    <td>%d</td>\n"
    "    <td>%s:%d</td>\n"
    "    <td>%s</td>\n"
    "    <td>%d</td>\n"
    "</tr>\n"
    "</table>\n"
    "\n"
    "%s" /* Active clients (nothing in restart page) */
    "%s" /* Usage guide table */
    "</div>\n"
    "<div id=\"footer\">udpxy v. %s (Build %d) %s - [%s]<br>%s</div>\n"   /* footer */
    "</div>\n"
    "</body>\n"
    "</html>\n";

static const char* ACLIENT_TABLE[] = {
    "<h3>Active clients:</h3>\n"
    "<table cellspacing=\"0\" class=\"proctabl\">\n"
    "<tr><th>Process ID</th><th>Source</th><th>Destination</th><th>Throughput</th></tr>\n",
    "</table>\n" };


static const char* ACLIENT_REC_FMT[] = {
    "<tr><td>%d</td><td>%s:%d</td><td>%s:%d%s</td><td>%s</td></tr>\n",
    "<tr class=\"uneven\"><td>%d</td><td>%s:%d</td><td>%s:%d%s</td><td>%s</td></tr>\n" };


static const char REDIRECT_SCRIPT_FMT[] =
    "<script type=\"text/JavaScript\" language=\"Javascript\">"
    "function body_onload(){"
    "  var t=setTimeout(\"window.location = '/status'\",%u)"
    "}"
    "</script>";

static const char REQUEST_GUIDE[] =
    "<h3>Available HTTP requests:</h3>\n"
    "<table cellspacing=\"0\">\n"
        "<tr><th>Request template</th><th>Function</th></tr>\n"
        "<tr><td><small>http://<i>address:port</i>/udp/<i>mcast_addr:mport/</i></small></td>\n"
        "<td>Relay multicast traffic from mcast_addr:mport</td></tr>"
        "<tr><td><small>http://<i>address:port</i>/status/</small></td><td>Display udpxy status</td></tr>\n"
        "<tr><td><small>http://<i>address:port</i>/restart/</small></td><td>Restart udpxy</td></tr>\n"
    "</table>\n";


#ifdef __cplusplus
}
#endif

#endif /* STATPG_H_0110081157_ */

/* __EOF__ */

