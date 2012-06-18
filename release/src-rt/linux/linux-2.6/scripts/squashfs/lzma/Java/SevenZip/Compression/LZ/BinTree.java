package SevenZip.Compression.LZ;
import java.io.IOException;
		
public class BinTree extends InWindow
{
	int _cyclicBufferPos;
	int _cyclicBufferSize;
	int _historySize;
	int _matchMaxLen;
	
	int[] _son;
	
	int[] _hash;
	int[] _hash2;
	int[] _hash3;
	
	int _cutValue = 0xFF;
	
	boolean HASH_ARRAY = true;
	boolean HASH_BIG = false;
	
	static final int kHash3Size = 1 << 18;
	
	static final int kBT2HashSize = 1 << 16;
	static final int kBT4Hash2Size = 1 << 10;
	static final int kBT4Hash4Size = 1 << 20;
	static final int kBT4bHash4Size = 1 << 23;
	static final int kBT2NumHashDirectBytes = 2;
	static final int kBT4NumHashDirectBytes = 0;
	
	int kHash2Size = kBT4Hash2Size;
	int kNumHashDirectBytes = kBT4NumHashDirectBytes;
	int kNumHashBytes = 4;
	int kHashSize = kBT4Hash4Size;

	private static final int[] CrcTable = new int[256];
	
	static
	{
		for (int i = 0; i < 256; i++)
		{
			int r = i;
			for (int j = 0; j < 8; j++)
				if ((r & 1) != 0)
					r = (r >>> 1) ^ 0xEDB88320;
				else
					r >>>= 1;
			CrcTable[i] = r;
		}
	}
	
	public void SetType(int numHashBytes, boolean big)
	{
		HASH_ARRAY = numHashBytes > 2;
		HASH_BIG = big;
		if (HASH_ARRAY)
		{
			kHash2Size = kBT4Hash2Size;
			kNumHashDirectBytes = kBT4NumHashDirectBytes;
			kNumHashBytes = 4;
			kHashSize = (HASH_BIG ? kBT4bHash4Size : kBT4Hash4Size);
		}
		else
		{
			kNumHashDirectBytes = kBT2NumHashDirectBytes;
			kNumHashBytes = 2;
			kHashSize = kBT2HashSize;
		}
	}
	
	static final int kEmptyHashValue = 0;
	
	static final int kMaxValForNormalize = ((int)1 << 30) - 1;
	

	public void Init() throws IOException
	{
		super.Init();
		int i;
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
	
	public void MovePos() throws IOException
	{
		if (++_cyclicBufferPos >= _cyclicBufferSize)
			_cyclicBufferPos = 0;
		super.MovePos();
		if (_pos == kMaxValForNormalize)
			Normalize();
	}
	
	public boolean Create(int historySize, int keepAddBufferBefore,
			int matchMaxLen, int keepAddBufferAfter)
	{
		int windowReservSize = (historySize + keepAddBufferBefore +
				matchMaxLen + keepAddBufferAfter) / 2 + 256;
		
		_son = null;
		_hash = null;
		_hash2 = null;
		_hash3 = null;
		
		super.Create(historySize + keepAddBufferBefore, matchMaxLen + keepAddBufferAfter, windowReservSize);
		
		if (_blockSize + 256 > kMaxValForNormalize)
			return false;
		
		_historySize = historySize;
		_matchMaxLen = matchMaxLen;
		
		_cyclicBufferSize = historySize + 1;
		_son = new int[_cyclicBufferSize * 2];
		
		_hash = new int[kHashSize];
		if (HASH_ARRAY)
		{
			_hash2 = new int[kHash2Size];
			_hash3 = new int[kHash3Size];
		}
		return true;
	}
	
	public int GetLongestMatch(int[] distances)
	{
		int lenLimit;
		if (_pos + _matchMaxLen <= _streamPos)
			lenLimit = _matchMaxLen;
		else
		{
			lenLimit = _streamPos - _pos;
			if (lenLimit < kNumHashBytes)
				return 0;
		}
		
		int matchMinPos = (_pos > _historySize) ? (_pos - _historySize) : 1;
		
		int cur = _bufferOffset + _pos;
		
		int matchHashLenMax = 0;
		
		int hashValue, hash2Value = 0, hash3Value = 0;
		
		if (HASH_ARRAY)
		{
			int temp = CrcTable[_bufferBase[cur] & 0xFF] ^ (_bufferBase[cur + 1] & 0xFF);
			hash2Value = temp & (kHash2Size - 1);
			temp ^= ((int)(_bufferBase[cur + 2] & 0xFF) << 8);
			hash3Value = temp & (kHash3Size - 1);
			hashValue = (temp ^ (CrcTable[_bufferBase[cur + 3] & 0xFF] << 5)) & (kHashSize - 1);
		}
		else
			hashValue = ((_bufferBase[cur] & 0xFF) ^ ((int)(_bufferBase[cur + 1] & 0xFF) << 8)) & (kHashSize - 1);
		
		int curMatch = _hash[hashValue];
		int curMatch2 = 0, curMatch3 = 0;
		int len2Distance = 0;
		int len3Distance = 0;
		boolean matchLen2Exist = false;
		boolean matchLen3Exist = false;
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
		int ptrLeft = (_cyclicBufferPos << 1) + 1;
		int ptrRight = (_cyclicBufferPos << 1);
		
		int maxLen, minLeft, minRight;
		maxLen = minLeft = minRight = kNumHashDirectBytes;
		
		distances[maxLen] = _pos - curMatch - 1;
		
		for (int count = _cutValue; count != 0; count--)
		{
			int pby1 = _bufferOffset + curMatch;
			int currentLen = Math.min(minLeft, minRight);
			for (; currentLen < lenLimit; currentLen++)
				if (_bufferBase[pby1 + currentLen] != _bufferBase[cur + currentLen])
					break;
			int delta = _pos - curMatch;
			while (currentLen > maxLen)
				distances[++maxLen] = delta - 1;
			
			int cyclicPos = ((delta <= _cyclicBufferPos) ?
				(_cyclicBufferPos - delta) :
				(_cyclicBufferPos - delta + _cyclicBufferSize)) << 1;
			
			if (currentLen != lenLimit)
			{
				if ((_bufferBase[pby1 + currentLen] & 0xFF) < (_bufferBase[cur + currentLen] & 0xFF))
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
		
		int lenLimit;
		if (_pos + _matchMaxLen <= _streamPos)
			lenLimit = _matchMaxLen;
		else
		{
			lenLimit = _streamPos - _pos;
			if (lenLimit < kNumHashBytes)
				return;
		}
		
		int matchMinPos = (_pos > _historySize) ? (_pos - _historySize) : 1;
		
		int cur = _bufferOffset + _pos;
		
		int hashValue, hash2Value = 0, hash3Value = 0;
		
		if (HASH_ARRAY)
		{
			int temp = CrcTable[_bufferBase[cur] & 0xFF] ^ (_bufferBase[cur + 1] & 0xFF);
			hash2Value = temp & (kHash2Size - 1);
			temp ^= ((int)(_bufferBase[cur + 2] & 0xFF) << 8);
			hash3Value = temp & (kHash3Size - 1);
			hashValue = (temp ^ (CrcTable[_bufferBase[cur + 3] & 0xFF] << 5)) & (kHashSize - 1);
		}
		else
			hashValue = ((_bufferBase[cur] & 0xFF) ^ ((int)(_bufferBase[cur + 1] & 0xFF) << 8)) & (kHashSize - 1);
		
		int curMatch = _hash[hashValue];
		int curMatch2 = 0, curMatch3 = 0;
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
		int ptrLeft = (_cyclicBufferPos << 1) + 1;
		int ptrRight = (_cyclicBufferPos << 1);
		
		int minLeft, minRight;
		minLeft = minRight = kNumHashDirectBytes;
		
		for (int count = _cutValue; count != 0; count--)
		{
			int pby1 = _bufferOffset + curMatch;
			int currentLen = Math.min(minLeft, minRight);
			for (; currentLen < lenLimit; currentLen++)
				if (_bufferBase[pby1 + currentLen] != _bufferBase[cur + currentLen])
					break;
			int delta = _pos - curMatch;
			
			int cyclicPos = ((delta <= _cyclicBufferPos) ?
				(_cyclicBufferPos - delta) :
				(_cyclicBufferPos - delta + _cyclicBufferSize)) << 1;
			
			if (currentLen != lenLimit)
			{
				if ((_bufferBase[pby1 + currentLen] & 0xFF) < (_bufferBase[cur + currentLen] & 0xFF))
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
	
	void NormalizeLinks(int[] items, int numItems, int subValue)
	{
		for (int i = 0; i < numItems; i++)
		{
			int value = items[i];
			if (value <= subValue)
				value = kEmptyHashValue;
			else
				value -= subValue;
			items[i] = value;
		}
	}
	
	void Normalize()
	{
		int startItem = _pos - _historySize;
		int subValue = startItem - 1;
		NormalizeLinks(_son, _cyclicBufferSize * 2, subValue);
		
		NormalizeLinks(_hash, kHashSize, subValue);
		
		if (HASH_ARRAY)
		{
			NormalizeLinks(_hash2, kHash2Size, subValue);
			NormalizeLinks(_hash3, kHash3Size, subValue);
		}
		ReduceOffsets(subValue);
	}
	
	public void SetCutValue(int cutValue)
	{ _cutValue = cutValue; }
}
