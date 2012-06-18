// LzmaEncoder.cs

using System;

namespace SevenZip.Compression.LZMA
{
	using RangeCoder;

	public class Encoder : ICoder, ISetCoderProperties, IWriteCoderProperties
	{
		enum EMatchFinderType
		{
			BT2,
			BT4,
			BT4B
		};

		static string[] kMatchFinderIDs = 
		{
			"BT2",
			"BT4",
			"BT4B",
		};

		static int FindMatchFinder(string s)
		{
			for (int m = 0; m < kMatchFinderIDs.Length; m++)
				if (s == kMatchFinderIDs[m])
					return m;
			return -1;
		}

		const UInt32 kIfinityPrice = 0xFFFFFFF;

		static Byte[] g_FastPos = new Byte[1024];

		static Encoder()
		{
			const Byte kFastSlots = 20;
			int c = 2;
			g_FastPos[0] = 0;
			g_FastPos[1] = 1;
			for (Byte slotFast = 2; slotFast < kFastSlots; slotFast++)
			{
				UInt32 k = ((UInt32)1 << ((slotFast >> 1) - 1));
				for (UInt32 j = 0; j < k; j++, c++)
					g_FastPos[c] = slotFast;
			}
		}

		static UInt32 GetPosSlot(UInt32 pos)
		{
			if (pos < (1 << 10))
				return g_FastPos[pos];
			if (pos < (1 << 19))
				return (UInt32)(g_FastPos[pos >> 9] + 18);
			return (UInt32)(g_FastPos[pos >> 18] + 36);
		}

		static UInt32 GetPosSlot2(UInt32 pos)
		{
			if (pos < (1 << 16))
				return (UInt32)(g_FastPos[pos >> 6] + 12);
			if (pos < (1 << 25))
				return (UInt32)(g_FastPos[pos >> 15] + 30);
			return (UInt32)(g_FastPos[pos >> 24] + 48);
		}

		Base.State _state = new Base.State();
		Byte _previousByte;
		UInt32[] _repDistances = new UInt32[Base.kNumRepDistances];

		void BaseInit()
		{
			_state.Init();
			_previousByte = 0;
			for (UInt32 i = 0; i < Base.kNumRepDistances; i++)
				_repDistances[i] = 0;
		}

		const int kDefaultDictionaryLogSize = 20;
		const UInt32 kNumFastBytesDefault = 0x20;

		class LiteralEncoder
		{
			public struct Encoder2
			{
				BitEncoder[] m_Encoders;

				public void Create() { m_Encoders = new BitEncoder[0x300]; }

				public void Init() { for (int i = 0; i < 0x300; i++) m_Encoders[i].Init(); }

				public void Encode(RangeCoder.Encoder rangeEncoder, byte symbol)
				{
					uint context = 1;
					for (int i = 7; i >= 0; i--)
					{
						uint bit = (uint)((symbol >> i) & 1);
						m_Encoders[context].Encode(rangeEncoder, bit);
						context = (context << 1) | bit;
					}
				}

				public void EncodeMatched(RangeCoder.Encoder rangeEncoder, byte matchByte, byte symbol)
				{
					uint context = 1;
					bool same = true;
					for (int i = 7; i >= 0; i--)
					{
						uint bit = (uint)((symbol >> i) & 1);
						uint state = context;
						if (same)
						{
							uint matchBit = (uint)((matchByte >> i) & 1);
							state += ((1 + matchBit) << 8);
							same = (matchBit == bit);
						}
						m_Encoders[state].Encode(rangeEncoder, bit);
						context = (context << 1) | bit;
					}
				}

				public uint GetPrice(bool matchMode, byte matchByte, byte symbol)
				{
					uint price = 0;
					uint context = 1;
					int i = 7;
					if (matchMode)
					{
						for (; i >= 0; i--)
						{
							uint matchBit = (uint)(matchByte >> i) & 1;
							uint bit = (uint)(symbol >> i) & 1;
							price += m_Encoders[((1 + matchBit) << 8) + context].GetPrice(bit);
							context = (context << 1) | bit;
							if (matchBit != bit)
							{
								i--;
								break;
							}
						}
					}
					for (; i >= 0; i--)
					{
						uint bit = (uint)(symbol >> i) & 1;
						price += m_Encoders[context].GetPrice(bit);
						context = (context << 1) | bit;
					}
					return price;
				}
			}

			Encoder2[] m_Coders;
			int m_NumPrevBits;
			int m_NumPosBits;
			uint m_PosMask;

			public void Create(int numPosBits, int numPrevBits)
			{
				if (m_Coders != null && m_NumPrevBits == numPrevBits &&
					m_NumPosBits == numPosBits)
					return;
				m_NumPosBits = numPosBits;
				m_PosMask = ((uint)1 << numPosBits) - 1;
				m_NumPrevBits = numPrevBits;
				uint numStates = (uint)1 << (m_NumPrevBits + m_NumPosBits);
				m_Coders = new Encoder2[numStates];
				for (uint i = 0; i < numStates; i++)
					m_Coders[i].Create();
			}

			public void Init()
			{
				uint numStates = (uint)1 << (m_NumPrevBits + m_NumPosBits);
				for (uint i = 0; i < numStates; i++)
					m_Coders[i].Init();
			}

			uint GetState(uint pos, byte prevByte)
			{ return ((pos & m_PosMask) << m_NumPrevBits) + (uint)(prevByte >> (8 - m_NumPrevBits)); }

			public Encoder2 GetSubCoder(UInt32 pos, Byte prevByte)
			{ return m_Coders[GetState(pos, prevByte)]; }
		}

		class LenEncoder
		{
			RangeCoder.BitEncoder _choice = new RangeCoder.BitEncoder();
			RangeCoder.BitEncoder _choice2 = new RangeCoder.BitEncoder();
			RangeCoder.BitTreeEncoder[] _lowCoder = new RangeCoder.BitTreeEncoder[Base.kNumPosStatesEncodingMax]; // kNumLowBits
			RangeCoder.BitTreeEncoder[] _midCoder = new RangeCoder.BitTreeEncoder[Base.kNumPosStatesEncodingMax]; // kNumMidBits
			RangeCoder.BitTreeEncoder _highCoder = new RangeCoder.BitTreeEncoder(Base.kNumHighLenBits);

			public LenEncoder()
			{
				for (UInt32 posState = 0; posState < Base.kNumPosStatesEncodingMax; posState++)
				{
					_lowCoder[posState] = new RangeCoder.BitTreeEncoder(Base.kNumLowLenBits);
					_midCoder[posState] = new RangeCoder.BitTreeEncoder(Base.kNumMidLenBits);
				}
			}

			public void Init(UInt32 numPosStates)
			{
				_choice.Init();
				_choice2.Init();
				for (UInt32 posState = 0; posState < numPosStates; posState++)
				{
					_lowCoder[posState].Init();
					_midCoder[posState].Init();
				}
				_highCoder.Init();
			}

			public void Encode(RangeCoder.Encoder rangeEncoder, UInt32 symbol, UInt32 posState)
			{
				if (symbol < Base.kNumLowLenSymbols)
				{
					_choice.Encode(rangeEncoder, 0);
					_lowCoder[posState].Encode(rangeEncoder, symbol);
				}
				else
				{
					symbol -= Base.kNumLowLenSymbols;
					_choice.Encode(rangeEncoder, 1);
					if (symbol < Base.kNumMidLenSymbols)
					{
						_choice2.Encode(rangeEncoder, 0);
						_midCoder[posState].Encode(rangeEncoder, symbol);
					}
					else
					{
						_choice2.Encode(rangeEncoder, 1);
						_highCoder.Encode(rangeEncoder, symbol - Base.kNumMidLenSymbols);
					}
				}
			}

			public UInt32 GetPrice(UInt32 symbol, UInt32 posState)
			{
				UInt32 price = 0;
				if (symbol < Base.kNumLowLenSymbols)
				{
					price += _choice.GetPrice0();
					price += _lowCoder[posState].GetPrice(symbol);
				}
				else
				{
					symbol -= Base.kNumLowLenSymbols;
					price += _choice.GetPrice1();
					if (symbol < Base.kNumMidLenSymbols)
					{
						price += _choice2.GetPrice0();
						price += _midCoder[posState].GetPrice(symbol);
					}
					else
					{
						price += _choice2.GetPrice1();
						price += _highCoder.GetPrice(symbol - Base.kNumMidLenSymbols);
					}
				}
				return price;
			}
		};

		const UInt32 kNumLenSpecSymbols = Base.kNumLowLenSymbols + Base.kNumMidLenSymbols;

		class LenPriceTableEncoder : LenEncoder
		{
			UInt32[] _prices = new UInt32[Base.kNumLenSymbols << Base.kNumPosStatesBitsEncodingMax];
			UInt32 _tableSize;
			UInt32[] _counters = new UInt32[Base.kNumPosStatesEncodingMax];

			public void SetTableSize(UInt32 tableSize) { _tableSize = tableSize; }

			public new UInt32 GetPrice(UInt32 symbol, UInt32 posState)
			{
				return _prices[(symbol << Base.kNumPosStatesBitsEncodingMax) + posState];
			}

			void UpdateTable(UInt32 posState)
			{
				for (UInt32 len = 0; len < _tableSize; len++)
					_prices[(len << Base.kNumPosStatesBitsEncodingMax) + posState] = base.GetPrice(len, posState);
				_counters[posState] = _tableSize;
			}

			public void UpdateTables(UInt32 numPosStates)
			{
				for (UInt32 posState = 0; posState < numPosStates; posState++)
					UpdateTable(posState);
			}

			public new void Encode(RangeCoder.Encoder rangeEncoder, UInt32 symbol, UInt32 posState)
			{
				base.Encode(rangeEncoder, symbol, posState);
				if (--_counters[posState] == 0)
					UpdateTable(posState);
			}
		}

		const UInt32 kNumOpts = 1 << 12;
		class Optimal
		{
			public Base.State State;

			public bool Prev1IsChar;
			public bool Prev2;

			public UInt32 PosPrev2;
			public UInt32 BackPrev2;

			public UInt32 Price;
			public UInt32 PosPrev;         // posNext;
			public UInt32 BackPrev;

			// public UInt32 []Backs = new UInt32[Base.kNumRepDistances];
			public UInt32 Backs0;
			public UInt32 Backs1;
			public UInt32 Backs2;
			public UInt32 Backs3;

			public void MakeAsChar() { BackPrev = 0xFFFFFFFF; Prev1IsChar = false; }
			public void MakeAsShortRep() { BackPrev = 0; ; Prev1IsChar = false; }
			public bool IsShortRep() { return (BackPrev == 0); }
		};
		Optimal[] _optimum = new Optimal[kNumOpts];

		LZ.IMatchFinder _matchFinder = null; // test it
		RangeCoder.Encoder _rangeEncoder = new RangeCoder.Encoder();

		RangeCoder.BitEncoder[] _isMatch = new RangeCoder.BitEncoder[Base.kNumStates << Base.kNumPosStatesBitsMax];
		RangeCoder.BitEncoder[] _isRep = new RangeCoder.BitEncoder[Base.kNumStates];
		RangeCoder.BitEncoder[] _isRepG0 = new RangeCoder.BitEncoder[Base.kNumStates];
		RangeCoder.BitEncoder[] _isRepG1 = new RangeCoder.BitEncoder[Base.kNumStates];
		RangeCoder.BitEncoder[] _isRepG2 = new RangeCoder.BitEncoder[Base.kNumStates];
		RangeCoder.BitEncoder[] _isRep0Long = new RangeCoder.BitEncoder[Base.kNumStates << Base.kNumPosStatesBitsMax];

		RangeCoder.BitTreeEncoder[] _posSlotEncoder = new RangeCoder.BitTreeEncoder[Base.kNumLenToPosStates]; // kNumPosSlotBits
		RangeCoder.BitEncoder[] _posEncoders = new RangeCoder.BitEncoder[Base.kNumFullDistances - Base.kEndPosModelIndex];
		RangeCoder.BitTreeEncoder _posAlignEncoder = new RangeCoder.BitTreeEncoder(Base.kNumAlignBits);

		LenPriceTableEncoder _lenEncoder = new LenPriceTableEncoder();
		LenPriceTableEncoder _repMatchLenEncoder = new LenPriceTableEncoder();

		LiteralEncoder _literalEncoder = new LiteralEncoder();

		UInt32[] _matchDistances = new UInt32[Base.kMatchMaxLen + 1];
		bool _fastMode = false;
		bool _maxMode = false;
		UInt32 _numFastBytes = kNumFastBytesDefault;
		UInt32 _longestMatchLength;
		UInt32 _additionalOffset;

		UInt32 _optimumEndIndex;
		UInt32 _optimumCurrentIndex;

		bool _longestMatchWasFound;

		UInt32[] _posSlotPrices = new UInt32[Base.kDistTableSizeMax << Base.kNumLenToPosStatesBits];
		UInt32[] _distancesPrices = new UInt32[Base.kNumFullDistances << Base.kNumLenToPosStatesBits];
		UInt32[] _alignPrices = new UInt32[Base.kAlignTableSize];
		UInt32 _alignPriceCount;

		UInt32 _distTableSize = (kDefaultDictionaryLogSize * 2);

		int _posStateBits = 2;
		UInt32 _posStateMask = (4 - 1);
		int _numLiteralPosStateBits = 0;
		int _numLiteralContextBits = 3;

		UInt32 _dictionarySize = (1 << kDefaultDictionaryLogSize);
		UInt32 _dictionarySizePrev = 0xFFFFFFFF;
		UInt32 _numFastBytesPrev = 0xFFFFFFFF;

		Int64 lastPosSlotFillingPos;
		Int64 nowPos64;
		bool _finished;

		System.IO.Stream _inStream;
		EMatchFinderType _matchFinderType = EMatchFinderType.BT4;
		bool _writeEndMark;
		bool _needReleaseMFStream;

		void Create()
		{
			// _rangeEncoder.Create(1 << 20);
			if (_matchFinder == null)
			{
				LZ.BinTree bt = new LZ.BinTree();
				int numHashBytes = 4;
				bool big = false;
				switch (_matchFinderType)
				{
					case EMatchFinderType.BT2:
						numHashBytes = 2;
						break;
					case EMatchFinderType.BT4:
						break;
					case EMatchFinderType.BT4B:
						big = true;
						break;
				}
				bt.SetType(numHashBytes, big);
				_matchFinder = bt;
			}
			_literalEncoder.Create(_numLiteralPosStateBits, _numLiteralContextBits);

			if (_dictionarySize == _dictionarySizePrev && _numFastBytesPrev == _numFastBytes)
				return;
			_matchFinder.Create(_dictionarySize, kNumOpts, _numFastBytes,
				Base.kMatchMaxLen * 2 + 1 - _numFastBytes);
			_dictionarySizePrev = _dictionarySize;
			_numFastBytesPrev = _numFastBytes;
		}

		public Encoder()
		{
			for (int i = 0; i < kNumOpts; i++)
				_optimum[i] = new Optimal();
			for (int i = 0; i < Base.kNumLenToPosStates; i++)
				_posSlotEncoder[i] = new RangeCoder.BitTreeEncoder(Base.kNumPosSlotBits);
		}

		void SetWriteEndMarkerMode(bool writeEndMarker)
		{
			_writeEndMark = writeEndMarker;
		}
		void Init()
		{
			BaseInit();
			_rangeEncoder.Init();

			uint i;
			for (i = 0; i < Base.kNumStates; i++)
			{
				for (uint j = 0; j <= _posStateMask; j++)
				{
					uint complexState = (i << Base.kNumPosStatesBitsMax) + j;
					_isMatch[complexState].Init();
					_isRep0Long[complexState].Init();
				}
				_isRep[i].Init();
				_isRepG0[i].Init();
				_isRepG1[i].Init();
				_isRepG2[i].Init();
			}
			_literalEncoder.Init();
			for (i = 0; i < Base.kNumLenToPosStates; i++)
				_posSlotEncoder[i].Init();
			for (i = 0; i < Base.kNumFullDistances - Base.kEndPosModelIndex; i++)
				_posEncoders[i].Init();

			_lenEncoder.Init((UInt32)1 << _posStateBits);
			_repMatchLenEncoder.Init((UInt32)1 << _posStateBits);

			_posAlignEncoder.Init();

			_longestMatchWasFound = false;
			_optimumEndIndex = 0;
			_optimumCurrentIndex = 0;
			_additionalOffset = 0;
		}

		void ReadMatchDistances(out UInt32 lenRes)
		{
			lenRes = _matchFinder.GetLongestMatch(_matchDistances);
			if (lenRes == _numFastBytes)
				lenRes += _matchFinder.GetMatchLen((int)lenRes, _matchDistances[lenRes],
						Base.kMatchMaxLen - lenRes);
			_additionalOffset++;
			_matchFinder.MovePos();
		}

		void MovePos(UInt32 num)
		{
			for (; num > 0; num--)
			{
				_matchFinder.DummyLongestMatch();
				_matchFinder.MovePos();
				_additionalOffset++;
			}
		}

		UInt32 GetRepLen1Price(Base.State state, UInt32 posState)
		{
			return _isRepG0[state.Index].GetPrice0() +
					_isRep0Long[(state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice0();
		}

		UInt32 GetRepPrice(UInt32 repIndex, UInt32 len, Base.State state, UInt32 posState)
		{
			UInt32 price = _repMatchLenEncoder.GetPrice(len - Base.kMatchMinLen, posState);
			if (repIndex == 0)
			{
				price += _isRepG0[state.Index].GetPrice0();
				price += _isRep0Long[(state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice1();
			}
			else
			{
				price += _isRepG0[state.Index].GetPrice1();
				if (repIndex == 1)
					price += _isRepG1[state.Index].GetPrice0();
				else
				{
					price += _isRepG1[state.Index].GetPrice1();
					price += _isRepG2[state.Index].GetPrice(repIndex - 2);
				}
			}
			return price;
		}

		UInt32 GetPosLenPrice(UInt32 pos, UInt32 len, UInt32 posState)
		{
			if (len == 2 && pos >= 0x80)
				return kIfinityPrice;
			UInt32 price;
			UInt32 lenToPosState = Base.GetLenToPosState(len);
			if (pos < Base.kNumFullDistances)
				price = _distancesPrices[(pos << Base.kNumLenToPosStatesBits) + lenToPosState];
			else
				price = _posSlotPrices[(GetPosSlot2(pos) << Base.kNumLenToPosStatesBits) + lenToPosState] +
					_alignPrices[pos & Base.kAlignMask];
			return price + _lenEncoder.GetPrice(len - Base.kMatchMinLen, posState);
		}

		UInt32 Backward(out UInt32 backRes, UInt32 cur)
		{
			_optimumEndIndex = cur;
			UInt32 posMem = _optimum[cur].PosPrev;
			UInt32 backMem = _optimum[cur].BackPrev;
			do
			{
				if (_optimum[cur].Prev1IsChar)
				{
					_optimum[posMem].MakeAsChar();
					_optimum[posMem].PosPrev = posMem - 1;
					if (_optimum[cur].Prev2)
					{
						_optimum[posMem - 1].Prev1IsChar = false;
						_optimum[posMem - 1].PosPrev = _optimum[cur].PosPrev2;
						_optimum[posMem - 1].BackPrev = _optimum[cur].BackPrev2;
					}
				}
				UInt32 posPrev = posMem;
				UInt32 backCur = backMem;

				backMem = _optimum[posPrev].BackPrev;
				posMem = _optimum[posPrev].PosPrev;

				_optimum[posPrev].BackPrev = backCur;
				_optimum[posPrev].PosPrev = cur;
				cur = posPrev;
			}
			while (cur > 0);
			backRes = _optimum[0].BackPrev;
			_optimumCurrentIndex = _optimum[0].PosPrev;
			return _optimumCurrentIndex;
		}


		UInt32[] reps = new UInt32[Base.kNumRepDistances];
		UInt32[] repLens = new UInt32[Base.kNumRepDistances];

		UInt32 GetOptimum(UInt32 position, out UInt32 backRes)
		{
			if (_optimumEndIndex != _optimumCurrentIndex)
			{
				UInt32 lenRes = _optimum[_optimumCurrentIndex].PosPrev - _optimumCurrentIndex;
				backRes = _optimum[_optimumCurrentIndex].BackPrev;
				_optimumCurrentIndex = _optimum[_optimumCurrentIndex].PosPrev;
				return lenRes;
			}
			_optimumCurrentIndex = 0;
			_optimumEndIndex = 0; // test it;

			UInt32 lenMain;
			if (!_longestMatchWasFound)
			{
				ReadMatchDistances(out lenMain);
			}
			else
			{
				lenMain = _longestMatchLength;
				_longestMatchWasFound = false;
			}

			UInt32 repMaxIndex = 0;
			UInt32 i;
			for (i = 0; i < Base.kNumRepDistances; i++)
			{
				reps[i] = _repDistances[i];
				repLens[i] = _matchFinder.GetMatchLen(0 - 1, reps[i], Base.kMatchMaxLen);
				if (i == 0 || repLens[i] > repLens[repMaxIndex])
					repMaxIndex = i;
			}
			if (repLens[repMaxIndex] >= _numFastBytes)
			{
				backRes = repMaxIndex;
				UInt32 lenRes = repLens[repMaxIndex];
				MovePos(lenRes - 1);
				return lenRes;
			}

			if (lenMain >= _numFastBytes)
			{
				UInt32 backMain = (lenMain < _numFastBytes) ? _matchDistances[lenMain] :
					_matchDistances[_numFastBytes];
				backRes = backMain + Base.kNumRepDistances;
				MovePos(lenMain - 1);
				return lenMain;
			}
			Byte currentByte = _matchFinder.GetIndexByte(0 - 1);

			_optimum[0].State = _state;

			Byte matchByte;

			matchByte = _matchFinder.GetIndexByte((Int32)(0 - _repDistances[0] - 1 - 1));

			UInt32 posState = (position & _posStateMask);

			_optimum[1].Price = _isMatch[(_state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice0() +
				_literalEncoder.GetSubCoder(position, _previousByte).GetPrice(!_state.IsCharState(), matchByte, currentByte);
			_optimum[1].MakeAsChar();

			_optimum[1].PosPrev = 0;

			_optimum[0].Backs0 = reps[0];
			_optimum[0].Backs1 = reps[1];
			_optimum[0].Backs2 = reps[2];
			_optimum[0].Backs3 = reps[3];


			UInt32 matchPrice = _isMatch[(_state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice1();
			UInt32 repMatchPrice = matchPrice + _isRep[_state.Index].GetPrice1();

			if (matchByte == currentByte)
			{
				UInt32 shortRepPrice = repMatchPrice + GetRepLen1Price(_state, posState);
				if (shortRepPrice < _optimum[1].Price)
				{
					_optimum[1].Price = shortRepPrice;
					_optimum[1].MakeAsShortRep();
				}
			}
			if (lenMain < 2)
			{
				backRes = _optimum[1].BackPrev;
				return 1;
			}


			UInt32 normalMatchPrice = matchPrice + _isRep[_state.Index].GetPrice0();

			if (lenMain <= repLens[repMaxIndex])
				lenMain = 0;

			UInt32 len;
			for (len = 2; len <= lenMain; len++)
			{
				_optimum[len].PosPrev = 0;
				_optimum[len].BackPrev = _matchDistances[len] + Base.kNumRepDistances;
				_optimum[len].Price = normalMatchPrice +
					GetPosLenPrice(_matchDistances[len], len, posState);
				_optimum[len].Prev1IsChar = false;
			}

			if (lenMain < repLens[repMaxIndex])
				lenMain = repLens[repMaxIndex];

			for (; len <= lenMain; len++)
				_optimum[len].Price = kIfinityPrice;

			for (i = 0; i < Base.kNumRepDistances; i++)
			{
				UInt32 repLen = repLens[i];
				for (UInt32 lenTest = 2; lenTest <= repLen; lenTest++)
				{
					UInt32 curAndLenPrice = repMatchPrice + GetRepPrice(i, lenTest, _state, posState);
					Optimal optimum = _optimum[lenTest];
					if (curAndLenPrice < optimum.Price)
					{
						optimum.Price = curAndLenPrice;
						optimum.PosPrev = 0;
						optimum.BackPrev = i;
						optimum.Prev1IsChar = false;
					}
				}
			}

			UInt32 cur = 0;
			UInt32 lenEnd = lenMain;

			while (true)
			{
				cur++;
				if (cur == lenEnd)
				{
					return Backward(out backRes, cur);
				}
				position++;
				UInt32 posPrev = _optimum[cur].PosPrev;
				Base.State state;
				if (_optimum[cur].Prev1IsChar)
				{
					posPrev--;
					if (_optimum[cur].Prev2)
					{
						state = _optimum[_optimum[cur].PosPrev2].State;
						if (_optimum[cur].BackPrev2 < Base.kNumRepDistances)
							state.UpdateRep();
						else
							state.UpdateMatch();
					}
					else
						state = _optimum[posPrev].State;
					state.UpdateChar();
				}
				else
					state = _optimum[posPrev].State;
				if (posPrev == cur - 1)
				{
					if (_optimum[cur].IsShortRep())
						state.UpdateShortRep();
					else
						state.UpdateChar();
				}
				else
				{
					UInt32 pos;
					if (_optimum[cur].Prev1IsChar && _optimum[cur].Prev2)
					{
						posPrev = _optimum[cur].PosPrev2;
						pos = _optimum[cur].BackPrev2;
						state.UpdateRep();
					}
					else
					{
						pos = _optimum[cur].BackPrev;
						if (pos < Base.kNumRepDistances)
							state.UpdateRep();
						else
							state.UpdateMatch();
					}
					Optimal opt = _optimum[posPrev];
					if (pos < Base.kNumRepDistances)
					{
						if (pos == 0)
						{
							reps[0] = opt.Backs0;
							reps[1] = opt.Backs1;
							reps[2] = opt.Backs2;
							reps[3] = opt.Backs3;
						}
						else if (pos == 1)
						{
							reps[0] = opt.Backs1;
							reps[1] = opt.Backs0;
							reps[2] = opt.Backs2;
							reps[3] = opt.Backs3;
						}
						else if (pos == 2)
						{
							reps[0] = opt.Backs2;
							reps[1] = opt.Backs0;
							reps[2] = opt.Backs1;
							reps[3] = opt.Backs3;
						}
						else
						{
							reps[0] = opt.Backs3;
							reps[1] = opt.Backs0;
							reps[2] = opt.Backs1;
							reps[3] = opt.Backs2;
						}
					}
					else
					{
						reps[0] = (pos - Base.kNumRepDistances);
						reps[1] = opt.Backs0;
						reps[2] = opt.Backs1;
						reps[3] = opt.Backs2;
					}
				}
				_optimum[cur].State = state;
				_optimum[cur].Backs0 = reps[0];
				_optimum[cur].Backs1 = reps[1];
				_optimum[cur].Backs2 = reps[2];
				_optimum[cur].Backs3 = reps[3];
				UInt32 newLen;
				ReadMatchDistances(out newLen);
				if (newLen >= _numFastBytes)
				{
					_longestMatchLength = newLen;
					_longestMatchWasFound = true;
					return Backward(out backRes, cur);
				}
				UInt32 curPrice = _optimum[cur].Price;

				currentByte = _matchFinder.GetIndexByte(0 - 1);
				matchByte = _matchFinder.GetIndexByte((Int32)(0 - reps[0] - 1 - 1));

				posState = (position & _posStateMask);

				UInt32 curAnd1Price = curPrice +
					_isMatch[(state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice0() +
					_literalEncoder.GetSubCoder(position, _matchFinder.GetIndexByte(0 - 2)).
					GetPrice(!state.IsCharState(), matchByte, currentByte);

				Optimal nextOptimum = _optimum[cur + 1];

				bool nextIsChar = false;
				if (curAnd1Price < nextOptimum.Price)
				{
					nextOptimum.Price = curAnd1Price;
					nextOptimum.PosPrev = cur;
					nextOptimum.MakeAsChar();
					nextIsChar = true;
				}

				matchPrice = curPrice + _isMatch[(state.Index << Base.kNumPosStatesBitsMax) + posState].GetPrice1();
				repMatchPrice = matchPrice + _isRep[state.Index].GetPrice1();

				if (matchByte == currentByte &&
					!(nextOptimum.PosPrev < cur && nextOptimum.BackPrev == 0))
				{
					UInt32 shortRepPrice = repMatchPrice + GetRepLen1Price(state, posState);
					if (shortRepPrice <= nextOptimum.Price)
					{
						nextOptimum.Price = shortRepPrice;
						nextOptimum.PosPrev = cur;
						nextOptimum.MakeAsShortRep();
						// nextIsChar = false;
					}
				}
				/*
				if(newLen == 2 && _matchDistances[2] >= kDistLimit2) // test it maybe set 2000 ?
					continue;
				*/

				UInt32 numAvailableBytesFull = _matchFinder.GetNumAvailableBytes() + 1;
				numAvailableBytesFull = Math.Min(kNumOpts - 1 - cur, numAvailableBytesFull);
				UInt32 numAvailableBytes = numAvailableBytesFull;

				if (numAvailableBytes < 2)
					continue;
				if (numAvailableBytes > _numFastBytes)
					numAvailableBytes = _numFastBytes;
				if (numAvailableBytes >= 3 && !nextIsChar)
				{
					UInt32 backOffset = reps[0] + 1;
					UInt32 temp;
					for (temp = 1; temp < numAvailableBytes; temp++)
						// if (data[temp] != data[(size_t)temp - backOffset])
						if (_matchFinder.GetIndexByte((Int32)temp - 1) != _matchFinder.GetIndexByte((Int32)temp - (Int32)backOffset - 1))
							break;
					UInt32 lenTest2 = temp - 1;
					if (lenTest2 >= 2)
					{
						Base.State state2 = state;
						state2.UpdateChar();
						UInt32 posStateNext = (position + 1) & _posStateMask;
						UInt32 nextRepMatchPrice = curAnd1Price +
							_isMatch[(state2.Index << Base.kNumPosStatesBitsMax) + posStateNext].GetPrice1() +
							_isRep[state2.Index].GetPrice1();
						// for (; lenTest2 >= 2; lenTest2--)
						{
							while (lenEnd < cur + 1 + lenTest2)
								_optimum[++lenEnd].Price = kIfinityPrice;
							UInt32 curAndLenPrice = nextRepMatchPrice + GetRepPrice(
								0, lenTest2, state2, posStateNext);
							Optimal optimum = _optimum[cur + 1 + lenTest2];
							if (curAndLenPrice < optimum.Price)
							{
								optimum.Price = curAndLenPrice;
								optimum.PosPrev = cur + 1;
								optimum.BackPrev = 0;
								optimum.Prev1IsChar = true;
								optimum.Prev2 = false;
							}
						}
					}
				}
				for (UInt32 repIndex = 0; repIndex < Base.kNumRepDistances; repIndex++)
				{
					// UInt32 repLen = _matchFinder->GetMatchLen(0 - 1, reps[repIndex], newLen); // test it;
					UInt32 backOffset = reps[repIndex] + 1;
					if (_matchFinder.GetIndexByte(-1) != 
						  _matchFinder.GetIndexByte((Int32)(-1 - (Int32)backOffset)) ||
							_matchFinder.GetIndexByte(0) != 
							_matchFinder.GetIndexByte((Int32)(0 - (Int32)backOffset)))
						continue;
					UInt32 lenTest;
					for (lenTest = 2; lenTest < numAvailableBytes; lenTest++)
						if (_matchFinder.GetIndexByte((Int32)lenTest - 1) != 
						    _matchFinder.GetIndexByte((Int32)lenTest - 1 - (Int32)backOffset))
							break;
					UInt32 lenTestTemp = lenTest;
				  do
					{
						while (lenEnd < cur + lenTest)
							_optimum[++lenEnd].Price = kIfinityPrice;
						UInt32 curAndLenPrice = repMatchPrice + GetRepPrice(repIndex, lenTest, state, posState);
						Optimal optimum = _optimum[cur + lenTest];
						if (curAndLenPrice < optimum.Price)
						{
							optimum.Price = curAndLenPrice;
							optimum.PosPrev = cur;
							optimum.BackPrev = repIndex;
							optimum.Prev1IsChar = false;
						}
					}
					while(--lenTest >= 2);
					lenTest = lenTestTemp;

					if (_maxMode)
					{
						UInt32 lenTest2 = lenTest + 1;
						UInt32 limit = Math.Min(numAvailableBytesFull, lenTest2 + _numFastBytes);
						for (; lenTest2 < limit; lenTest2++)
							if (_matchFinder.GetIndexByte((Int32)((Int32)lenTest2 - 1)) !=
									_matchFinder.GetIndexByte((Int32)((Int32)lenTest2 - 1 - (Int32)backOffset)))
								break;
						lenTest2 -= lenTest + 1;
						if (lenTest2 >= 2)
						{
							Base.State state2 = state;
							state2.UpdateRep();
							UInt32 posStateNext = (position + lenTest) & _posStateMask;
							UInt32 curAndLenCharPrice = 
									repMatchPrice + GetRepPrice(repIndex, lenTest, state, posState) + 
									_isMatch[(state2.Index << Base.kNumPosStatesBitsMax) + posStateNext].GetPrice0() +
									_literalEncoder.GetSubCoder(position + lenTest, 
											_matchFinder.GetIndexByte((Int32)lenTest - 1 - 1)).GetPrice(true,
											_matchFinder.GetIndexByte((Int32)((Int32)lenTest - 1 - (Int32)backOffset)), 
											_matchFinder.GetIndexByte((Int32)lenTest - 1));
							state2.UpdateChar();
							posStateNext = (position + lenTest + 1) & _posStateMask;
							UInt32 nextMatchPrice = curAndLenCharPrice + _isMatch[(state2.Index << Base.kNumPosStatesBitsMax) + posStateNext].GetPrice1();
							UInt32 nextRepMatchPrice = nextMatchPrice + _isRep[state2.Index].GetPrice1();
	            
							// for(; lenTest2 >= 2; lenTest2--)
							{
								UInt32 offset = lenTest + 1 + lenTest2;
								while(lenEnd < cur + offset)
									_optimum[++lenEnd].Price = kIfinityPrice;
								UInt32 curAndLenPrice = nextRepMatchPrice + GetRepPrice(0, lenTest2, state2, posStateNext);
								Optimal optimum = _optimum[cur + offset];
								if (curAndLenPrice < optimum.Price) 
								{
									optimum.Price = curAndLenPrice;
									optimum.PosPrev = cur + lenTest + 1;
									optimum.BackPrev = 0;
									optimum.Prev1IsChar = true;
									optimum.Prev2 = true;
									optimum.PosPrev2 = cur;
									optimum.BackPrev2 = repIndex;
								}
							}
						}
					}
				}

				//    for(UInt32 lenTest = 2; lenTest <= newLen; lenTest++)
				if (newLen > numAvailableBytes)
					newLen = numAvailableBytes;
				if (newLen >= 2)
				{
					if (newLen == 2 && _matchDistances[2] >= 0x80)
						continue;
					normalMatchPrice = matchPrice +
						_isRep[state.Index].GetPrice0();
					while (lenEnd < cur + newLen)
						_optimum[++lenEnd].Price = kIfinityPrice;

					for (UInt32 lenTest = newLen; lenTest >= 2; lenTest--)
					{
						UInt32 curBack = _matchDistances[lenTest];
						UInt32 curAndLenPrice = normalMatchPrice + GetPosLenPrice(curBack, lenTest, posState);
						Optimal optimum = _optimum[cur + lenTest];
						if (curAndLenPrice < optimum.Price)
						{
							optimum.Price = curAndLenPrice;
							optimum.PosPrev = cur;
							optimum.BackPrev = curBack + Base.kNumRepDistances;
							optimum.Prev1IsChar = false;
						}

						if (_maxMode && (lenTest == newLen || curBack != _matchDistances[lenTest + 1]))
						{
							UInt32 backOffset = curBack + 1;
							UInt32 lenTest2 = lenTest + 1;
							UInt32 limit = Math.Min(numAvailableBytesFull, lenTest2 + _numFastBytes);
							for (; lenTest2 < limit; lenTest2++)
								if (_matchFinder.GetIndexByte((Int32)lenTest2 - 1) != _matchFinder.GetIndexByte((Int32)lenTest2 - (Int32)backOffset - 1))
									break;
							lenTest2 -= lenTest + 1;
							if (lenTest2 >= 2)
							{
								Base.State state2 = state;
								state2.UpdateMatch();
								UInt32 posStateNext = (position + lenTest) & _posStateMask;
								UInt32 curAndLenCharPrice = curAndLenPrice +
									_isMatch[(state2.Index << Base.kNumPosStatesBitsMax) + posStateNext].GetPrice0() +
									_literalEncoder.GetSubCoder(position + lenTest,
									// data[(size_t)lenTest - 1], 
									_matchFinder.GetIndexByte((Int32)lenTest - 1 - 1)).
									GetPrice(true,
									// data[(size_t)lenTest - backOffset], data[lenTest]
									_matchFinder.GetIndexByte((Int32)lenTest - (Int32)backOffset - 1),
									_matchFinder.GetIndexByte((Int32)lenTest - 1)
									);
								state2.UpdateChar();
								posStateNext = (position + lenTest + 1) & _posStateMask;
								UInt32 nextMatchPrice = curAndLenCharPrice + _isMatch[(state2.Index << Base.kNumPosStatesBitsMax) + posStateNext].GetPrice1();
								UInt32 nextRepMatchPrice = nextMatchPrice + _isRep[state2.Index].GetPrice1();

								// for(; lenTest2 >= 2; lenTest2--)
								{
									UInt32 offset = lenTest + 1 + lenTest2;
									while (lenEnd < cur + offset)
										_optimum[++lenEnd].Price = kIfinityPrice;
									curAndLenPrice = nextRepMatchPrice + GetRepPrice(
										0, lenTest2, state2, posStateNext);
									optimum = _optimum[cur + offset];
									if (curAndLenPrice < optimum.Price)
									{
										optimum.Price = curAndLenPrice;
										optimum.PosPrev = cur + lenTest + 1;
										optimum.BackPrev = 0;
										optimum.Prev1IsChar = true;
										optimum.Prev2 = true;
										optimum.PosPrev2 = cur;
										optimum.BackPrev2 = curBack + Base.kNumRepDistances;
									}
								}
							}
						}
					}
				}
			}
		}

		bool ChangePair(UInt32 smallDist, UInt32 bigDist)
		{
			const int kDif = 7;
			return (smallDist < ((UInt32)(1) << (32 - kDif)) && bigDist >= (smallDist << kDif));
		}

		UInt32 GetOptimumFast(UInt32 position, out UInt32 backRes)
		{
			UInt32 lenMain;
			if (!_longestMatchWasFound)
			{
				ReadMatchDistances(out lenMain);
			}
			else
			{
				lenMain = _longestMatchLength;
				_longestMatchWasFound = false;
			}
			UInt32 repMaxIndex = 0;
			for (UInt32 i = 0; i < Base.kNumRepDistances; i++)
			{
				repLens[i] = _matchFinder.GetMatchLen(0 - 1, _repDistances[i], Base.kMatchMaxLen);
				if (i == 0 || repLens[i] > repLens[repMaxIndex])
					repMaxIndex = i;
			}
			if (repLens[repMaxIndex] >= _numFastBytes)
			{
				backRes = repMaxIndex;
				UInt32 lenRes = repLens[repMaxIndex];
				MovePos(lenRes - 1);
				return lenRes;
			}
			if (lenMain >= _numFastBytes)
			{
				backRes = _matchDistances[_numFastBytes] + Base.kNumRepDistances;
				MovePos(lenMain - 1);
				return lenMain;
			}
			while (lenMain > 2)
			{
				if (!ChangePair(_matchDistances[lenMain - 1], _matchDistances[lenMain]))
					break;
				lenMain--;
			}
			if (lenMain == 2 && _matchDistances[2] >= 0x80)
				lenMain = 1;

			UInt32 backMain = _matchDistances[lenMain];
			if (repLens[repMaxIndex] >= 2)
			{
				if (repLens[repMaxIndex] + 1 >= lenMain ||
					repLens[repMaxIndex] + 2 >= lenMain && (backMain > (1 << 12)))
				{
					backRes = repMaxIndex;
					UInt32 lenRes = repLens[repMaxIndex];
					MovePos(lenRes - 1);
					return lenRes;
				}
			}

			if (lenMain >= 2)
			{
				ReadMatchDistances(out _longestMatchLength);
				if (_longestMatchLength >= 2 &&
					(
					(_longestMatchLength >= lenMain &&
					_matchDistances[lenMain] < backMain) ||
					_longestMatchLength == lenMain + 1 &&
					!ChangePair(backMain, _matchDistances[_longestMatchLength]) ||
					_longestMatchLength > lenMain + 1 ||
					_longestMatchLength + 1 >= lenMain && lenMain >= 3 &&
					ChangePair(_matchDistances[lenMain - 1], backMain)
					)
					)
				{
					_longestMatchWasFound = true;
					backRes = 0xFFFFFFFF;
					return 1;
				}
				for (UInt32 i = 0; i < Base.kNumRepDistances; i++)
				{
					UInt32 repLen = _matchFinder.GetMatchLen(0 - 1, _repDistances[i], Base.kMatchMaxLen);
					if (repLen >= 2 && repLen + 1 >= lenMain)
					{
						_longestMatchWasFound = true;
						backRes = 0xFFFFFFFF;
						return 1;
					}
				}
				backRes = backMain + Base.kNumRepDistances;
				MovePos(lenMain - 2);
				return lenMain;
			}
			backRes = 0xFFFFFFFF;
			return 1;
		}

		void InitMatchFinder(LZ.IMatchFinder matchFinder)
		{
			_matchFinder = matchFinder;
		}

		void WriteEndMarker(UInt32 posState)
		{
			if (!_writeEndMark)
				return;

			_isMatch[(_state.Index << Base.kNumPosStatesBitsMax) + posState].Encode(_rangeEncoder, 1);
			_isRep[_state.Index].Encode(_rangeEncoder, 0);
			_state.UpdateMatch();
			UInt32 len = Base.kMatchMinLen; // kMatchMaxLen;
			_lenEncoder.Encode(_rangeEncoder, len - Base.kMatchMinLen, posState);
			UInt32 posSlot = (1 << Base.kNumPosSlotBits) - 1;
			UInt32 lenToPosState = Base.GetLenToPosState(len);
			_posSlotEncoder[lenToPosState].Encode(_rangeEncoder, posSlot);
			int footerBits = 30;
			UInt32 posReduced = (((UInt32)1) << footerBits) - 1;
			_rangeEncoder.EncodeDirectBits(posReduced >> Base.kNumAlignBits, footerBits - Base.kNumAlignBits);
			_posAlignEncoder.ReverseEncode(_rangeEncoder, posReduced & Base.kAlignMask);
		}

		void Flush(UInt32 nowPos)
		{
			ReleaseMFStream();
			WriteEndMarker(nowPos & _posStateMask);
			_rangeEncoder.FlushData();
			_rangeEncoder.FlushStream();
		}

		public void CodeOneBlock(out Int64 inSize, out Int64 outSize, out bool finished)
		{
			inSize = 0;
			outSize = 0;
			finished = true;

			if (_inStream != null)
			{
				_matchFinder.Init(_inStream);
				_needReleaseMFStream = true;
				_inStream = null;
			}

			if (_finished)
				return;
			_finished = true;


			Int64 progressPosValuePrev = nowPos64;
			if (nowPos64 == 0)
			{
				if (_matchFinder.GetNumAvailableBytes() == 0)
				{
					Flush((UInt32)nowPos64);
					return;
				}
				UInt32 len; // it's not used
				ReadMatchDistances(out len);
				UInt32 posState = (UInt32)(nowPos64) & _posStateMask;
				_isMatch[(_state.Index << Base.kNumPosStatesBitsMax) + posState].Encode(_rangeEncoder, 0);
				_state.UpdateChar();
				Byte curByte = _matchFinder.GetIndexByte((Int32)(0 - _additionalOffset));
				_literalEncoder.GetSubCoder((UInt32)(nowPos64), _previousByte).Encode(_rangeEncoder, curByte);
				_previousByte = curByte;
				_additionalOffset--;
				nowPos64++;
			}
			if (_matchFinder.GetNumAvailableBytes() == 0)
			{
				Flush((UInt32)nowPos64);
				return;
			}
			while (true)
			{
				UInt32 pos;
				UInt32 posState = ((UInt32)nowPos64) & _posStateMask;

				UInt32 len;
				if (_fastMode)
					len = GetOptimumFast((UInt32)nowPos64, out pos);
				else
					len = GetOptimum((UInt32)nowPos64, out pos);

				UInt32 complexState = (_state.Index << Base.kNumPosStatesBitsMax) + posState;
				if (len == 1 && pos == 0xFFFFFFFF)
				{
					_isMatch[complexState].Encode(_rangeEncoder, 0);
					Byte curByte = _matchFinder.GetIndexByte((Int32)(0 - _additionalOffset));
					LiteralEncoder.Encoder2 subCoder = _literalEncoder.GetSubCoder((UInt32)nowPos64, _previousByte);
					if (!_state.IsCharState())
					{
						Byte matchByte = _matchFinder.GetIndexByte((Int32)(0 - _repDistances[0] - 1 - _additionalOffset));
						subCoder.EncodeMatched(_rangeEncoder, matchByte, curByte);
					}
					else
						subCoder.Encode(_rangeEncoder, curByte);
					_previousByte = curByte;
					_state.UpdateChar();
				}
				else
				{
					_isMatch[complexState].Encode(_rangeEncoder, 1);
					if (pos < Base.kNumRepDistances)
					{
						_isRep[_state.Index].Encode(_rangeEncoder, 1);
						if (pos == 0)
						{
							_isRepG0[_state.Index].Encode(_rangeEncoder, 0);
							if (len == 1)
								_isRep0Long[complexState].Encode(_rangeEncoder, 0);
							else
								_isRep0Long[complexState].Encode(_rangeEncoder, 1);
						}
						else
						{
							_isRepG0[_state.Index].Encode(_rangeEncoder, 1);
							if (pos == 1)
								_isRepG1[_state.Index].Encode(_rangeEncoder, 0);
							else
							{
								_isRepG1[_state.Index].Encode(_rangeEncoder, 1);
								_isRepG2[_state.Index].Encode(_rangeEncoder, pos - 2);
							}
						}
						if (len == 1)
							_state.UpdateShortRep();
						else
						{
							_repMatchLenEncoder.Encode(_rangeEncoder, len - Base.kMatchMinLen, posState);
							_state.UpdateRep();
						}


						UInt32 distance = _repDistances[pos];
						if (pos != 0)
						{
							for (UInt32 i = pos; i >= 1; i--)
								_repDistances[i] = _repDistances[i - 1];
							_repDistances[0] = distance;
						}
					}
					else
					{
						_isRep[_state.Index].Encode(_rangeEncoder, 0);
						_state.UpdateMatch();
						_lenEncoder.Encode(_rangeEncoder, len - Base.kMatchMinLen, posState);
						pos -= Base.kNumRepDistances;
						UInt32 posSlot = GetPosSlot(pos);
						UInt32 lenToPosState = Base.GetLenToPosState(len);
						_posSlotEncoder[lenToPosState].Encode(_rangeEncoder, posSlot);

						if (posSlot >= Base.kStartPosModelIndex)
						{
							int footerBits = (int)((posSlot >> 1) - 1);
							UInt32 baseVal = ((2 | (posSlot & 1)) << footerBits);
							UInt32 posReduced = pos - baseVal;

							if (posSlot < Base.kEndPosModelIndex)
								RangeCoder.BitTreeEncoder.ReverseEncode(_posEncoders,
										baseVal - posSlot - 1, _rangeEncoder, footerBits, posReduced);
							else
							{
								_rangeEncoder.EncodeDirectBits(posReduced >> Base.kNumAlignBits, footerBits - Base.kNumAlignBits);
								_posAlignEncoder.ReverseEncode(_rangeEncoder, posReduced & Base.kAlignMask);
								if (!_fastMode)
									if (--_alignPriceCount == 0)
										FillAlignPrices();
							}
						}
						UInt32 distance = pos;
						for (UInt32 i = Base.kNumRepDistances - 1; i >= 1; i--)
							_repDistances[i] = _repDistances[i - 1];
						_repDistances[0] = distance;
					}
					_previousByte = _matchFinder.GetIndexByte((Int32)(len - 1 - _additionalOffset));
				}
				_additionalOffset -= len;
				nowPos64 += len;
				if (!_fastMode)
					if (nowPos64 - lastPosSlotFillingPos >= (1 << 9))
					{
						FillPosSlotPrices();
						FillDistancesPrices();
						lastPosSlotFillingPos = nowPos64;
					}
				if (_additionalOffset == 0)
				{
					inSize = nowPos64;
					outSize = _rangeEncoder.GetProcessedSizeAdd();
					if (_matchFinder.GetNumAvailableBytes() == 0)
					{
						Flush((UInt32)nowPos64);
						return;
					}

					if (nowPos64 - progressPosValuePrev >= (1 << 12))
					{
						_finished = false;
						finished = false;
						return;
					}
				}
			}
		}

		void ReleaseMFStream()
		{
			if (_matchFinder != null && _needReleaseMFStream)
			{
				_matchFinder.ReleaseStream();
				_needReleaseMFStream = false;
			}
		}

		void SetOutStream(System.IO.Stream outStream) { _rangeEncoder.SetStream(outStream); }
		void ReleaseOutStream() { _rangeEncoder.ReleaseStream(); }

		void ReleaseStreams()
		{
			ReleaseMFStream();
			ReleaseOutStream();
		}

		void SetStreams(System.IO.Stream inStream, System.IO.Stream outStream,
				Int64 inSize, Int64 outSize)
		{
			_inStream = inStream;
			_finished = false;
			Create();
			SetOutStream(outStream);
			Init();

			// CCoderReleaser releaser(this);

			/*
			if (_matchFinder->GetNumAvailableBytes() == 0)
				return Flush();
			*/

			if (!_fastMode)
			{
				FillPosSlotPrices();
				FillDistancesPrices();
				FillAlignPrices();
			}

			_lenEncoder.SetTableSize(_numFastBytes + 1 - Base.kMatchMinLen);
			_lenEncoder.UpdateTables((UInt32)1 << _posStateBits);
			_repMatchLenEncoder.SetTableSize(_numFastBytes + 1 - Base.kMatchMinLen);
			_repMatchLenEncoder.UpdateTables((UInt32)1 << _posStateBits);

			lastPosSlotFillingPos = 0;
			nowPos64 = 0;
		}

		public void Code(System.IO.Stream inStream, System.IO.Stream outStream,
			Int64 inSize, Int64 outSize, ICodeProgress progress)
		{
			_needReleaseMFStream = false;
			try
			{
				SetStreams(inStream, outStream, inSize, outSize);
				while (true)
				{
					Int64 processedInSize;
					Int64 processedOutSize;
					bool finished;
					CodeOneBlock(out processedInSize, out processedOutSize, out finished);
					if (finished)
						return;
					if (progress != null)
					{
						progress.SetProgress(processedInSize, processedOutSize);
					}
				}
			}
			finally
			{
				ReleaseStreams();
			}
		}

		public void SetCoderProperties(CoderPropID[] propIDs, object[] properties)
		{
			for (UInt32 i = 0; i < properties.Length; i++)
			{
				object prop = properties[i];
				switch (propIDs[i])
				{
					case CoderPropID.NumFastBytes:
						{
							if (!(prop is Int32))
								throw new InvalidParamException();
							Int32 numFastBytes = (Int32)prop;
							if (numFastBytes < 5 || numFastBytes > Base.kMatchMaxLen)
								throw new InvalidParamException();
							_numFastBytes = (UInt32)numFastBytes;
							break;
						}
					case CoderPropID.Algorithm:
						{
							if (!(prop is Int32))
								throw new InvalidParamException();
							Int32 maximize = (Int32)prop;
							_fastMode = (maximize == 0);
							_maxMode = (maximize >= 2);
							break;
						}
					case CoderPropID.MatchFinder:
						{
							if (!(prop is String))
								throw new InvalidParamException();
							EMatchFinderType matchFinderIndexPrev = _matchFinderType;
							int m = FindMatchFinder(((string)prop).ToUpper());
							if (m < 0)
								throw new InvalidParamException();
							_matchFinderType = (EMatchFinderType)m;
							if (_matchFinder != null && matchFinderIndexPrev != _matchFinderType)
							{
								_dictionarySizePrev = 0xFFFFFFFF;
								_matchFinder = null;
							}
							break;
						}
					case CoderPropID.DictionarySize:
						{
							const int kDicLogSizeMaxCompress = 28;
							if (!(prop is Int32))
								throw new InvalidParamException(); ;
							Int32 dictionarySize = (Int32)prop;
							if (dictionarySize < (UInt32)(1 << Base.kDicLogSizeMin) ||
								dictionarySize > (UInt32)(1 << kDicLogSizeMaxCompress))
								throw new InvalidParamException();
							_dictionarySize = (UInt32)dictionarySize;
							int dicLogSize;
							for (dicLogSize = 0; dicLogSize < (UInt32)kDicLogSizeMaxCompress; dicLogSize++)
								if (dictionarySize <= ((UInt32)(1) << dicLogSize))
									break;
							_distTableSize = (UInt32)dicLogSize * 2;
							break;
						}
					case CoderPropID.PosStateBits:
						{
							if (!(prop is Int32))
								throw new InvalidParamException();
							Int32 v = (Int32)prop;
							if (v < 0 || v > (UInt32)Base.kNumPosStatesBitsEncodingMax)
								throw new InvalidParamException();
							_posStateBits = (int)v;
							_posStateMask = (((UInt32)1) << (int)_posStateBits) - 1;
							break;
						}
					case CoderPropID.LitPosBits:
						{
							if (!(prop is Int32))
								throw new InvalidParamException();
							Int32 v = (Int32)prop;
							if (v < 0 || v > (UInt32)Base.kNumLitPosStatesBitsEncodingMax)
								throw new InvalidParamException();
							_numLiteralPosStateBits = (int)v;
							break;
						}
					case CoderPropID.LitContextBits:
						{
							if (!(prop is Int32))
								throw new InvalidParamException();
							Int32 v = (Int32)prop;
							if (v < 0 || v > (UInt32)Base.kNumLitContextBitsMax)
								throw new InvalidParamException(); ;
							_numLiteralContextBits = (int)v;
							break;
						}
					case CoderPropID.EndMarker:
						{
							if (!(prop is Boolean))
								throw new InvalidParamException();
							SetWriteEndMarkerMode((Boolean)prop);
							break;
						}
					default:
						throw new InvalidParamException();
				}
			}
		}

		const int kPropSize = 5;
		Byte[] properties = new Byte[kPropSize];

		public void WriteCoderProperties(System.IO.Stream outStream)
		{
			properties[0] = (Byte)((_posStateBits * 5 + _numLiteralPosStateBits) * 9 + _numLiteralContextBits);
			for (int i = 0; i < 4; i++)
				properties[1 + i] = (Byte)(_dictionarySize >> (8 * i));
			outStream.Write(properties, 0, kPropSize);
		}

		void FillPosSlotPrices()
		{
			for (UInt32 lenToPosState = 0; lenToPosState < Base.kNumLenToPosStates; lenToPosState++)
			{
				UInt32 posSlot;
				for (posSlot = 0; posSlot < Base.kEndPosModelIndex && posSlot < _distTableSize; posSlot++)
					_posSlotPrices[(posSlot << Base.kNumLenToPosStatesBits) + lenToPosState] = _posSlotEncoder[lenToPosState].GetPrice(posSlot);
				for (; posSlot < _distTableSize; posSlot++)
					_posSlotPrices[(posSlot << Base.kNumLenToPosStatesBits) + lenToPosState] = _posSlotEncoder[lenToPosState].GetPrice(posSlot) +
						((((posSlot >> 1) - 1) - Base.kNumAlignBits) << RangeCoder.BitEncoder.kNumBitPriceShiftBits);
			}
		}

		void FillDistancesPrices()
		{
			for (UInt32 lenToPosState = 0; lenToPosState < Base.kNumLenToPosStates; lenToPosState++)
			{
				UInt32 i;
				for (i = 0; i < Base.kStartPosModelIndex; i++)
					_distancesPrices[(i << Base.kNumLenToPosStatesBits) + lenToPosState] = _posSlotPrices[(i << Base.kNumLenToPosStatesBits) + lenToPosState];
				for (; i < Base.kNumFullDistances; i++)
				{
					UInt32 posSlot = GetPosSlot(i);
					int footerBits = (int)((posSlot >> 1) - 1);
					UInt32 baseVal = ((2 | (posSlot & 1)) << footerBits);

					_distancesPrices[(i << Base.kNumLenToPosStatesBits) + lenToPosState] = _posSlotPrices[(posSlot << Base.kNumLenToPosStatesBits) + lenToPosState] +
					RangeCoder.BitTreeEncoder.ReverseGetPrice(_posEncoders,
						baseVal - posSlot - 1, footerBits, i - baseVal);
				}
			}
		}

		void FillAlignPrices()
		{
			for (UInt32 i = 0; i < Base.kAlignTableSize; i++)
				_alignPrices[i] = _posAlignEncoder.ReverseGetPrice(i);
			_alignPriceCount = Base.kAlignTableSize;
		}
	}
}
