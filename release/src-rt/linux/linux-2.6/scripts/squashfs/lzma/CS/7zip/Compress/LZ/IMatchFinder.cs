// IMatchFinder.cs

using System;

namespace SevenZip.Compression.LZ
{
	interface IInWindowStream
	{
		void Init(System.IO.Stream inStream);
		void ReleaseStream();
		void MovePos();
		Byte GetIndexByte(Int32 index);
		UInt32 GetMatchLen(Int32 index, UInt32 distance, UInt32 limit);
		UInt32 GetNumAvailableBytes();
	}

	interface IMatchFinder : IInWindowStream
	{
		void Create(UInt32 historySize, UInt32 keepAddBufferBefore,
				UInt32 matchMaxLen, UInt32 keepAddBufferAfter);
		UInt32 GetLongestMatch(UInt32[] distances);
		void DummyLongestMatch();
	}
}
