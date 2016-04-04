/*
 * jPlayer Plugin for jQuery JavaScript Library
 * http://www.jplayer.org
 *
 * Copyright (c) 2009 - 2014 Happyworm Ltd
 * Licensed under the MIT license.
 * http://opensource.org/licenses/MIT
 *
 * Author: Robert M. Hall
 * Date: 7th August 2012
 */

// This class was found to cause problems on OSX with Firefox and Safari where more than 8 instances of the SWF are on a page.

package happyworm.jPlayer
{
	import flash.net.LocalConnection;
	import flash.events.StatusEvent;
	import flash.system.Capabilities;
	import flash.utils.getTimer;

	public class TraceOut
	{

		private var outgoing_lc:LocalConnection = new LocalConnection ();
		private var firstEvent:Boolean = true;
		private var _localAIRDebug:Boolean = false;

		public function TraceOut()
		{
			outgoing_lc.addEventListener(StatusEvent.STATUS, lcListener);
			outgoing_lc.send("_log_output","startLogging","");
		}

		private function lcListener(event:StatusEvent):void
		{
			// Must have this listener to avoid errors
			if (event.level == "error")
			{
				_localAIRDebug = false;
			}
			else if(event.level =="status" && firstEvent==true)
			{
				firstEvent = false;
				tracer("<< Successful Connection To Event Logger >>");
				tracer("DEBUG INFO: \n<"+Capabilities.serverString + ">\nFlash Player Version: " + Capabilities.version + "\n");
				_localAIRDebug = true;
			}
		}

		public function localAIRDebug():Boolean
		{
			return _localAIRDebug;
		}

		public function tracer(msg:String):void
		{
			trace(msg);
			var outMsg:String = "[" + getTimer() + "ms] " + msg;
			outgoing_lc.send("_log_output","displayMsg",outMsg);
							 
		}
	}
}
