For Each Host In WScript.Arguments
	Set WMIservice = GetObject("winmgmts:{impersonationLevel=impersonate}!\\" & host & "\root\cimv2")

	Set colsettings = WMIservice.ExecQuery("SELECT * FROM Win32_Processor")


	For Each proc In colsettings
		Wscript.Echo(host & ": " & proc.description)
	Next
Next
