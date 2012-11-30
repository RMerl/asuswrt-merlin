<% UI_cloud_status(); %>
enable_cloudsync = '<% nvram_get("enable_cloudsync"); %>';
cloud_synclist_array = decodeURIComponent('<% nvram_show_chinese_char("cloud_sync"); %>').replace(/>/g, "&#62").replace(/</g, "&#60");
