#ifndef __BITSTREAM_H_
#define __BITSTREAM_H_

#define KPackedFrameLen 10
#define KUnpackedFrameLen 22

// Below values are taken from the APS design document
const TUint8 KG729FullPayloadBits[] = { 8, 10, 8, 1, 13, 4, 7, 5, 13, 4, 7 };
const TUint  KNumFullFrameParams = 11;
const TUint8 KG729SIDPayloadBits[] = { 1, 5, 4, 5 };
const TUint  KNumSIDFrameParams = 4;

/*! 
  @class TBitStream
  
  @discussion Provides compression from 16-bit-word-aligned G.729 audio frames
  (used in S60 G.729 DSP codec) to 8-bit stream, and vice versa.
  */
class TBitStream
  {        
public:
    /*!
      @function TBitStream
      
      @discussion Constructor
      */   
    TBitStream():iDes(iData,KUnpackedFrameLen){}
    /*!
      @function CompressG729Frame
      
      @discussion Compress either a 22-byte G.729 full rate frame to 10 bytes
      or a 8-byte G.729 Annex.B SID frame to 2 bytes.
      @param aSrc Reference to the uncompressed source frame data
      @param aIsSIDFrame True if the source is a SID frame
      @result a reference to the compressed frame
      */
    const TDesC8& CompressG729Frame( const TDesC8& aSrc, TBool aIsSIDFrame = EFalse );
    
    /*!
      @function ExpandG729Frame
            
      @discussion Expand a 10-byte G.729 full rate frame to 22 bytes
      or a 2-byte G.729 Annex.B SID frame to 8(22) bytes.
      @param aSrc Reference to the compressed source frame data
      @param aIsSIDFrame True if the source is a SID frame
      @result a reference to a descriptor representing the uncompressed frame.
      Note that SID frames are zero-padded to 22 bytes as well.
      */
    const TDesC8& ExpandG729Frame( const TDesC8& aSrc, TBool aIsSIDFrame = EFalse );

private:
    void Compress( TUint8 aValue, TUint8 aNumOfBits );
    void Expand( const TUint8* aSrc, TInt aDstIdx, TUint8 aNumOfBits );

private:
    TUint8      iData[KUnpackedFrameLen];
    TPtr8       iDes;
    TInt        iIdx;
    TInt        iBitOffset;
    };
    

const TDesC8& TBitStream::CompressG729Frame( const TDesC8& aSrc, TBool aIsSIDFrame )
    {
    // reset data
    iDes.FillZ(iDes.MaxLength());
    iIdx = iBitOffset = 0;
    
    TInt numParams = (aIsSIDFrame) ? KNumSIDFrameParams : KNumFullFrameParams;
    const TUint8* p = const_cast<TUint8*>(aSrc.Ptr());
    
    for(TInt i = 0, pIdx = 0; i < numParams; i++, pIdx += 2) 
        {
        TUint8 paramBits = (aIsSIDFrame) ? KG729SIDPayloadBits[i] : KG729FullPayloadBits[i];        
        if(paramBits > 8)
            {
            Compress(p[pIdx+1], paramBits - 8); // msb
            paramBits = 8;
            }            
        Compress(p[pIdx], paramBits); // lsb    
        }

    if( iBitOffset )
        iIdx++;
        
    iDes.SetLength(iIdx);
    return iDes;
    }

 
const TDesC8& TBitStream::ExpandG729Frame( const TDesC8& aSrc, TBool aIsSIDFrame )
    {
    // reset data
    iDes.FillZ(iDes.MaxLength());
    iIdx = iBitOffset = 0;
    
    TInt numParams = (aIsSIDFrame) ? KNumSIDFrameParams : KNumFullFrameParams;
    const TUint8* p = const_cast<TUint8*>(aSrc.Ptr());
    
    for(TInt i = 0, dIdx = 0; i < numParams; i++, dIdx += 2) 
        {
        TUint8 paramBits = (aIsSIDFrame) ? KG729SIDPayloadBits[i] : KG729FullPayloadBits[i];
        if(paramBits > 8)
          {
          Expand(p, dIdx+1, paramBits - 8); // msb
          paramBits = 8;
          }    
        Expand(p, dIdx, paramBits); // lsb 
        }
        
    iDes.SetLength(KUnpackedFrameLen);
    return iDes;
    }


void TBitStream::Compress( TUint8 aValue, TUint8 aNumOfBits )
  {
    // clear bits that will be discarded
    aValue &= (0xff >> (8 - aNumOfBits));
    
    // calculate required bitwise left shift
    TInt shl = 8 - (iBitOffset + aNumOfBits);
    
    if (shl == 0) // no shift required
        { 
        iData[iIdx++] |= aValue;
        iBitOffset = 0;
        }
    else if (shl > 0) // bits fit into current byte
        { 
        iData[iIdx] |= (aValue << shl);
        iBitOffset += aNumOfBits;
        }        
    else        
        { 
        iBitOffset = -shl;
        iData[iIdx] |= (aValue >> iBitOffset); // right shift
        iData[++iIdx] |= (aValue << (8-iBitOffset)); // push remaining bits to next byte
        }
  }
    

void TBitStream::Expand( const TUint8* aSrc, TInt aDstIdx, TUint8 aNumOfBits )
  {    
    TUint8 aValue = aSrc[iIdx] & (0xff >> iBitOffset);
       
    // calculate required bitwise right shift
    TInt shr = 8 - (iBitOffset + aNumOfBits);
    
    if (shr == 0) // no shift required
        { 
        iData[aDstIdx] = aValue;
        iIdx++;
        iBitOffset = 0;
        }
    else if (shr > 0) // right shift
        { 
        iData[aDstIdx] = (aValue >> shr);
        iBitOffset += aNumOfBits;
        }        
    else // shift left and take remaining bits from the next src byte
        {
        iBitOffset = -shr;
        iData[aDstIdx] = aValue << iBitOffset;                
        iData[aDstIdx] |= aSrc[++iIdx] >> (8 - iBitOffset);
        }
  }

#endif // __BITSTREAM_H_
    
// eof
