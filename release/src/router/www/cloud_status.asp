cloud_synclist_array = decodeURIComponent('<% nvram_char_to_ascii("", "cloud_sync"); %>').replace(/>/g, "&#62").replace(/</g, "&#60");
<% UI_cloud_status(); %>
enable_cloudsync = '<% nvram_get("enable_cloudsync"); %>';