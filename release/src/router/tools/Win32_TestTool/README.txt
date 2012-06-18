How To use :

1. Start stress test
2. Run AsusWrtTestToolReport.exe
3.a Put HttpCmd.exe to a folder, run cmd.exe to invoke Windows console, goto the folder
3.b on the folder, type HttpCmd 192.168.1.1 index.asp 5 9999 test_result.txt

-------------------------------

AsusWrtTestToolReport is tool to collect info from test tool.

For HttpCmd, please ensure error count is 0.

-------------------------------

HttpCmd is a tool to send HTTP request to test router is alive or not.

If router crashs and not responds, it will show error when returns other code.

For good quality router, it need to have all normal return code, 200.

Please run this tool when stress test.

Example :

HttpCmd.exe 192.168.1.1 index.asp 5 9999 test_result.txt

192.168.1.1 : httpd server
index.asp : file to retrieve
5 : delay between two http request
9999 : 9999 = infinite loop, other number means loop in how many times
test_result.txt : result file

PS:
server username : admin
server password : admin

