/*
 * jPlayer Plugin for jQuery JavaScript Library
 * http://www.jplayer.org
 *
 * Copyright (c) 2009 - 2014 Happyworm Ltd
 * Licensed under the MIT license.
 * http://opensource.org/licenses/MIT
 *
 * Author: Mark J Panaghiston
 * Date: 2nd April 2014
 */

package happyworm.jPlayer {
	public class JplayerStatus {

		public static const VERSION:String = "2.6.0"; // The version of the Flash jPlayer entity.

		public var volume:Number = 0.5; // Not affected by reset()
		public var muted:Boolean = false; // Not affected by reset()

		public var src:String;
		public var srcError:Boolean;

		public var srcSet:Boolean;
		public var isPlaying:Boolean;
		public var isSeeking:Boolean;

		public var isWaiting:Boolean;

		public var playOnLoad:Boolean;
		public var playOnSeek:Boolean;

		public var isStartingDownload:Boolean;
		public var isLoading:Boolean;
		public var isLoaded:Boolean;

		public var pausePosition:Number;

		public var seekPercent:Number;
		public var currentTime:Number;
		public var currentPercentRelative:Number;
		public var currentPercentAbsolute:Number;
		public var duration:Number;

		public var videoWidth:Number;
		public var videoHeight:Number;
		
		public var metaDataReady:Boolean;
		public var metaData:Object;

		public function JplayerStatus() {
			reset();
		}
		public function reset():void {
			src = "";
			srcError = false;

			srcSet = false;
			isPlaying = false;
			isSeeking = false;

			isWaiting = false;

			playOnLoad = false;
			playOnSeek = false;

			isStartingDownload = false;
			isLoading = false;
			isLoaded = false;

			pausePosition = 0;

			seekPercent = 0;
			currentTime = 0;
			currentPercentRelative = 0;
			currentPercentAbsolute = 0;
			duration = 0;

			videoWidth = 0;
			videoHeight = 0;
			
			metaDataReady = false;
			metaData = {};
		}
		public function error():void {
			var srcSaved:String = src;
			reset();
			src = srcSaved;
			srcError = true;
		}
		public function loadRequired():Boolean {
			return (srcSet && !isStartingDownload && !isLoading && !isLoaded);
		}
		public function startingDownload():void {
			isStartingDownload = true;
			isLoading = false;
			isLoaded = false;
		}
		public function loading():void {
			isStartingDownload = false;
			isLoading = true;
			isLoaded = false;
		}
		public function loaded():void {
			isStartingDownload = false;
			isLoading = false;
			isLoaded = true;
		}
	}
}
