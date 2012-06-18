// LzBinTree.cs

using System;

namespace SevenZip.Compression.LZ
{
	public class BinTree : InWindow, IMatchFinder
	{
		UInt32 _cyclicBufferPos;
		UInt32 _cyclicBufferSize;
		UInt32 _historySize;
		UInt32 _matchMaxLen;

		// UInt32 []_dummy;
		UInt32[] _son;

		UInt32[] _hash;
		UInt32[] _hash2;
		UInt32[] _hash3;

		UInt32 _cutValue = 0xFF;

		bool HASH_ARRAY = true;
		bool HASH_BIG = false;

		const UInt32 kHash3Size = 1 << 18;

		const UInt32 kBT2HashSize = 1 << 16;
		const UInt32 kBT4Hash2Size = 1 << 10;
		const UInt32 kBT4Hash4Size = 1 << 20;
		const UInt32 kBT4bHash4Size = 1 << 23;
		const UInt32 kBT2NumHashDirectBytes = 2;
		const UInt32 kBT4NumHashDirectBytes = 0;

		UInt32 kHash2Size = kBT4Hash2Size;
		UInt32 kNumHashDirectBytes = kBT4NumHashDirectBytes;
		UInt32 kNumHashBytes = 4;
		UInt32 kHashSize = kBT4Hash4Size;

		public void SetType(int numHashBytes, bool big)
		{
			HASH_ARRAY = numHashBytes > 2;
			HASH_BIG = big;
			if (HASH_ARRAY)
			{
				kHash2Size = kBT4Hash2Size;
				kNumHashDirectBytes = kBT4NumHashDirectBytes;
				kNumHashBytes = 4;
				kHashSize = HASH_BIG ? kBT4bHash4Size : kBT4Hash4Size;
			}
			else
			{
				kNumHashDirectBytes = kBT2NumHashDirectBytes;
				kNumHashBytes = 2;
				kHashSize = kBT2HashSize;
			}
		}

		const UInt32 kEmptyHashValue = 0;

		const UInt32 kMaxValForNormalize = ((UInt32)1 << 31) - 1;

		UInt32 Hash(UInt32 offset, out UInt32 hash2Value, out UInt32 hash3Value)
		{
			UInt32 temp = CRC.Table[_bufferBase[offset]] ^ _bufferBase[offset + 1];
			hash2Value = temp & (kHash2Size - 1);
			temp ^= ((UInt32)(_bufferBase[offset + 2]) << 8);
			hash3Value = temp & (kHash3Size - 1);
			return (temp ^ (CRC.Table[_bufferBase[offset + 3]] << 5)) & (kHashSize - 1);
		}

		UInt32 Hash(UInt32 offset)
		{
			return _bufferBase[offset] ^ ((UInt32)(_bufferBase[offset + 1]) << 8);
		}

		public new void Init(System.IO.Stream inStream)
		{
			base.Init(inStream);
			UInt32 i;
			for (i = 0; i < kHashSize; i++)
				_hash[i] = kEmptyHashValue;
			if (HASH_ARRAY)
			{
				for (i = 0; i < kHash2Size; i++)
					_hash2[i] = kEmptyHashValue;
				for (i = 0; i < kHash3Size; i++)
					_hash3[i] = kEmptyHashValue;
			}
			_cyclicBufferPos = 0;
			ReduceOffsets(-1);
		}

		public new void ReleaseStream() { base.ReleaseStream(); }

		public new void MovePos()
		{
			_cyclicBufferPos++;
			if (_cyclicBufferPos >= _cyclicBufferSize)
				_cyclicBufferPos = 0;
			base.MovePos();
			if (_pos == kMaxValForNormalize)
				Normalize();
		}

		public new Byte GetIndexByte(Int32 index) { return base.GetIndexByte(index); }

		public new UInt32 GetMatchLen(Int32 index, UInt32 distance, UInt32 limit)
		{ return base.GetMatchLen(index, distance, limit); }

		public new UInt32 GetNumAvailableBytes() { return base.GetNumAvailableBytes(); }

		public void Create(UInt32 historySize, UInt32 keepAddBufferBefore,
			UInt32 matchMaxLen, UInt32 keepAddBufferAfter)
		{
			// _dummy = new UInt32[matchMaxLen + 1];
			UInt32 windowReservSize = (historySize + keepAddBufferBefore +
				matchMaxLen + keepAddBufferAfter) / 2 + 256;

			_son = null;
			_hash = null;
			_hash2 = null;
			_hash3 = null;

			base.Create(historySize + keepAddBufferBefore, matchMaxLen + keepAddBufferAfter, windowReservSize);

			if (_blockSize + 256 > kMaxValForNormalize)
				throw new Exception();

			_historySize = historySize;
			_matchMaxLen = matchMaxLen;

			_cyclicBufferSize = historySize + 1;
			_son = new UInt32[_cyclicBufferSize * 2];

			_hash = new UInt32[kHashSize];
			if (HASH_ARRAY)
			{
				_hash2 = new UInt32[kHash2Size];
				_hash3 = new UInt32[kHash3Size];
			}
		}

		public UInt32 GetLongestMatch(UInt32[] distances)
		{
			UInt32 lenLimit;
			if (_pos + _matchMaxLen <= _streamPos)
				lenLimit = _matchMaxLen;
			else
			{
				lenLimit = _streamPos - _pos;
				if (lenLimit < kNumHashBytes)
					return 0;
			}

			UInt32 matchMinPos = (_pos > _historySize) ? (_pos - _historySize) : 1;

			UInt32 cur = _bufferOffset + _pos;

			UInt32 matchHashLenMax = 0;

			UInt32 hashValue, hash2Value = 0, hash3Value = 0;

			if (HASH_ARRAY)
				hashValue = Hash(cur, out hash2Value, out hash3Value);
			else
				hashValue = Hash(cur);

			UInt32 curMatch = _hash[hashValue];
			UInt32 curMatch2 = 0, curMatch3 = 0;
			UInt32 len2Distance = 0;
			UInt32 len3Distance = 0;
			bool matchLen2Exist = false;
			bool matchLen3Exist = false;
			if (HASH_ARRAY)
			{
				curMatch2 = _hash2[hash2Value];
				curMatch3 = _hash3[hash3Value];
				_hash2[hash2Value] = _pos;
				if (curMatch2 >= matchMinPos)
				{
					if (_bufferBase[_bufferOffset + curMatch2] == _bufferBase[cur])
					{
						len2Distance = _pos - curMatch2 - 1;
						matchHashLenMax = 2;
						matchLen2Exist = true;
					}
				}
				{
					_hash3[hash3Value] = _pos;
					if (curMatch3 >= matchMinPos)
					{
						if (_bufferBase[_bufferOffset + curMatch3] == _bufferBase[cur])
						{
							len3Distance = _pos - curMatch3 - 1;
							matchHashLenMax = 3;
							matchLen3Exist = true;
							if (matchLen2Exist)
							{
								if (len3Distance < len2Distance)
									len2Distance = len3Distance;
							}
							else
							{
								len2Distance = len3Distance;
								matchLen2Exist = true;
							}
						}
					}
				}
			}

			_hash[hashValue] = _pos;

			if (curMatch < matchMinPos)
			{
				_son[(_cyclicBufferPos << 1)] = kEmptyHashValue;
				_son[(_cyclicBufferPos << 1) + 1] = kEmptyHashValue;

				if (HASH_ARRAY)
				{
					distances[2] = len2Distance;
					distances[3] = len3Distance;
				}
				return matchHashLenMax;
			}
			UInt32 ptrLeft = (_cyclicBufferPos << 1) + 1;
			UInt32 ptrRight = (_cyclicBufferPos << 1);

			UInt32 maxLen, minLeft, minRight;
			maxLen = minLeft = minRight = kNumHashDirectBytes;

			distances[maxLen] = _pos - curMatch - 1;

			for (UInt32 count = _cutValue; count > 0; count--)
			{
				UInt32 pby1 = _bufferOffset + curMatch;
				UInt32 currentLen = Math.Min(minLeft, minRight);
				for (; currentLen < lenLimit; currentLen++)
					if (_bufferBase[pby1 + currentLen] != _bufferBase[cur + currentLen])
						break;
				UInt32 delta = _pos - curMatch;
				while (currentLen > maxLen)
					distances[++maxLen] = delta - 1;

				UInt32 cyclicPos = ((delta <= _cyclicBufferPos) ?
							(_cyclicBufferPos - delta) :
							(_cyclicBufferPos - delta + _cyclicBufferSize)) << 1;

				if (currentLen != lenLimit)
				{
					if (_bufferBase[pby1 + currentLen] < _bufferBase[cur + currentLen])
					{
						_son[ptrRight] = curMatch;
						ptrRight = cyclicPos + 1;
						curMatch = _son[ptrRight];
						if (currentLen > minLeft)
							minLeft = currentLen;
					}
					else
					{
						_son[ptrLeft] = curMatch;
						ptrLeft = cyclicPos;
						curMatch = _son[ptrLeft];
						if (currentLen > minRight)
							minRight = currentLen;
					}
				}
				else
				{
					if (currentLen < _matchMaxLen)
					{
						_son[ptrLeft] = curMatch;
						ptrLeft = cyclicPos;
						curMatch = _son[ptrLeft];
						if (currentLen > minRight)
							minRight = currentLen;
					}
					else
					{
						_son[ptrLeft] = _son[cyclicPos + 1];
						_son[ptrRight] = _son[cyclicPos];

						if (HASH_ARRAY)
						{
							if (matchLen2Exist && len2Distance < distances[2])
								distances[2] = len2Distance;
							if (matchLen3Exist && len3Distance < distances[3])
								distances[3] = len3Distance;
						}
						return maxLen;
					}
				}
				if (curMatch < matchMinPos)
					break;
			}
			_son[ptrLeft] = kEmptyHashValue;
			_son[ptrRight] = kEmptyHashValue;

			if (HASH_ARRAY)
			{
				if (matchLen2Exist)
				{
					if (maxLen < 2)
					{
						distances[2] = len2Distance;
						maxLen = 2;
					}
					else if (len2Distance < distances[2])
						distances[2] = len2Distance;
				}
				{
					if (matchLen3Exist)
					{
						if (maxLen < 3)
						{
							distances[3] = len3Distance;
							maxLen = 3;
						}
						else if (len3Distance < distances[3])
							distances[3] = len3Distance;
					}
				}
			}
			return maxLen;
		}

		public void DummyLongestMatch()
		{
			// GetLongestMatch(_dummy);

			UInt32 lenLimit;
			if (_pos + _matchMaxLen <= _streamPos)
				lenLimit = _matchMaxLen;
			else
			{
				lenLimit = _streamPos - _pos;
				if (lenLimit < kNumHashBytes)
					return;
			}

			UInt32 matchMinPos = (_pos > _historySize) ? (_pos - _historySize) : 1;

			UInt32 cur = _bufferOffset + _pos;

			UInt32 hashValue, hash2Value = 0, hash3Value = 0;

			if (HASH_ARRAY)
				hashValue = Hash(cur, out hash2Value, out hash3Value);
			else
				hashValue = Hash(cur);

			UInt32 curMatch = _hash[hashValue];
			UInt32 curMatch2 = 0, curMatch3 = 0;
			if (HASH_ARRAY)
			{
				curMatch2 = _hash2[hash2Value];
				curMatch3 = _hash3[hash3Value];
				_hash2[hash2Value] = _pos;
				_hash3[hash3Value] = _pos;
			}
			_hash[hashValue] = _pos;

			if (curMatch < matchMinPos)
			{
				_son[(_cyclicBufferPos << 1)] = kEmptyHashValue;
				_son[(_cyclicBufferPos << 1) + 1] = kEmptyHashValue;
				return;
			}
			UInt32 ptrLeft = (_cyclicBufferPos << 1) + 1;
			UInt32 ptrRight = (_cyclicBufferPos << 1);

			UInt32 minLeft, minRight;
			minLeft = minRight = kNumHashDirectBytes;

			for (UInt32 count = _cutValue; count > 0; count--)
			{
				UInt32 pby1 = _bufferOffset + curMatch;
				UInt32 currentLen = Math.Min(minLeft, minRight);
				for (; currentLen < lenLimit; currentLen++)
					if (_bufferBase[pby1 + currentLen] != _bufferBase[cur + currentLen])
						break;
				UInt32 delta = _pos - curMatch;

				UInt32 cyclicPos = ((delta <= _cyclicBufferPos) ?
							(_cyclicBufferPos - delta) :
							(_cyclicBufferPos - delta + _cyclicBufferSize)) << 1;

				if (currentLen != lenLimit)
				{
					if (_bufferBase[pby1 + currentLen] < _bufferBase[cur + currentLen])
					{
						_son[ptrRight] = curMatch;
						ptrRight = cyclicPos + 1;
						curMatch = _son[ptrRight];
						if (currentLen > minLeft)
							minLeft = currentLen;
					}
					else
					{
						_son[ptrLeft] = curMatch;
						ptrLeft = cyclicPos;
						curMatch = _son[ptrLeft];
						if (currentLen > minRight)
							minRight = currentLen;
					}
				}
				else
				{
					if (currentLen < _matchMaxLen)
					{
						_son[ptrLeft] = curMatch;
						ptrLeft = cyclicPos;
						curMatch = _son[ptrLeft];
						if (currentLen > minRight)
							minRight = currentLen;
					}
					else
					{
						_son[ptrLeft] = _son[cyclicPos + 1];
						_son[ptrRight] = _son[cyclicPos];
						return;
					}
				}
				if (curMatch < matchMinPos)
					break;
			}
			_son[ptrLeft] = kEmptyHashValue;
			_son[ptrRight] = kEmptyHashValue;
		}

		void NormalizeLinks(UInt32[] items, UInt32 numItems, UInt32 subValue)
		{
			for (UInt32 i = 0; i < numItems; i++)
			{
				UInt32 value = items[i];
				if (value <= subValue)
					value = kEmptyHashValue;
				else
					value -= subValue;
				items[i] = value;
			}
		}

		void Normalize()
		{
			UInt32 startItem = _pos - _historySize;
			UInt32 subValue = startItem - 1;
			NormalizeLinks(_son, _cyclicBufferSize * 2, subValue);

			NormalizeLinks(_hash, kHashSize, subValue);

			if (HASH_ARRAY)
			{
				NormalizeLinks(_hash2, kHash2Size, subValue);
				NormalizeLinks(_hash3, kHash3Size, subValue);
			}
			ReduceOffsets((Int32)subValue);
		}

		public void SetCutValue(UInt32 cutValue) { _cutValue = cutValue; }
	}
}
