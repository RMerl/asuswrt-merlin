function dec2Bin(d) {
	var b = '';
	for (var i = 0; i < 8; i++) {
		b = (d%2) + b;
		d = Math.floor(d/2);
	}
	return b;
}

function str2hexstr(s) {
   var a,b,d;
   var hexStr = '';
   for (var i=0; i < s.length; i++) {
      d = s.charCodeAt(i);
      a = d % 16;
      b = (d - a)/16;
      hexStr += "0123456789ABCDEF".charAt(b) + "0123456789ABCDEF".charAt(a);
   }
   return hexStr;
}

function hex2ten( _v )
{
	var ret;
	
	if( _v == "A" || _v == "a" )
		ret = 10;
	else if( _v == "B" || _v == "b" )
		ret = 11;
	else if( _v == "C" || _v == "c" )
		ret = 12;
	else if( _v == "D" || _v == "d" )
		ret = 13;
	else if( _v == "E" || _v == "e" )
		ret = 14;
	else if( _v == "F" || _v == "f" )
		ret = 15;
	else
		ret = parseInt( _v );
	
	return ret;
}		

function ten2hex( _v )
{
	var ret;
	
	if( _v >=0 && _v <= 9 )
		ret = _v.toString();
	else if( _v == 10 )
		ret = "a";
	else if( _v == 11 )
		ret = "b";
	else if( _v == 12 )
		ret = "c";
	else if( _v == 13 )
		ret = "d";
	else if( _v == 14 )
		ret = "e";
	else if( _v == 15 )
		ret = "f";
	
	return ret;
}

function sumhexstr( str )
{	
	var ret = 0;
	
	var ic = Math.ceil( str.length / 4.0 );		
	for( var i = 0; i < ic; i++ )
	{
		var substr = str.substr( i*4, 4 );		
		var y = 0;
		for( var j = 0; j < substr.length; j++ )
		{
			var x = hex2ten( substr.charAt(j) ) * Math.pow( 16, substr.length - j - 1 );
			y += x;
		}
				
		ret += y;
	}
			
	return ret;
}
function getshiftamount( seed_str, orignaldostr )
{
	var seed_hex_str = str2hexstr( seed_str );	
	alert("seed_hex_str="+seed_hex_str);
	var sumhex = sumhexstr( seed_hex_str );	
	var shiftamount = sumhex % orignaldostr.length;
	alert(sumhex+", "+orignaldostr.length);	
	if( shiftamount == 0 )
		shiftamount = 15;
	return shiftamount;
}

function strleftshift( str, shiftamount )
{
	var aa = str.substring( shiftamount, str.length );
	var bb = str.substring( 0, shiftamount );
	return aa.concat(bb);
}
function deinterleave( src, charcnt )
{
	var ret = "";
	var ic = Math.ceil( src.length / charcnt );
	for( var i = 0; i < charcnt; i++ )
	{
		for( var j = 0; j < ic; j++ )
		{
			ret += src.charAt( (j * charcnt) + i );
		}
	}
	return ret;
}
function bin2ten( _v )
{
	var ret = 0;	
	for( var j = 0; j < _v.length; j++ )
	{
		var x = parseInt( _v.charAt(j) ) * Math.pow( 2, _v.length - j - 1 );
		ret += x;
	}
	return ret;
}
function ten2bin( _v )
{
	var binStr = "";	
	var _array = new Array(4);
	_array[0] = "0";
	_array[1] = "0";
	_array[2] = "0";
	_array[3] = "0";
	
	var _count = 3;
	while( _v > 0 )
  {
  	var sz;
		if( ( _v%2 ) == 0 )
    {
    	sz = "0";
    }
    else
    {
    	sz = "1";
    }
    _array[_count] = sz;
    _count--;	
    _v = Math.floor(_v/2);
	}
	
	for( var j = 0; j < _array.length; j++ )
	{
		binStr = binStr + _array[j];
	}
		
	return binStr;
}
function hexstr2binstr( in_src )
{
	var ret = "";
	var ic = in_src.length;
	for( var i = 0; i < ic; i++ )
	{
		ret = ret + ten2bin( hex2ten( in_src.charAt(i) ) );
	}				
	return ret;
}
function num2hex( _v )
{
	var ret = "";
				
	while( _v > 0 )
  {
  	var m = _v%16;
    ret = ten2hex(m) + ret;
    _v = Math.floor(_v/16);
  }		
	return ret;
}
function binstr2str( inbinstr )
{
	if( 0 != inbinstr.length % 8 )
	{
		return "";
	}		
	var ret = "";
	var ic = inbinstr.length / 8;
	var k = 0;				
	for( var i = 0; i < inbinstr.length; i+=8 )
	{		
		var substrupper = inbinstr.substr( i, 4 );
		var uc = bin2ten( substrupper );		
		var substrlowwer = inbinstr.substr( i + 4, 4 );
		var lc = bin2ten( substrlowwer );		
		var v = (uc << 4) | lc;		
		var charValue = String.fromCharCode(v);		
		ret += charValue;
		k++;
	}			
	return ret;
}
						
xt456={}
xt456.decode=function(in_str, key)
{
	if(in_str.indexOf("ASUSHARE")!=0)
		return;	
	in_str = in_str.substr(8, in_str.length);	
	var ibinstr = hexstr2binstr( in_str );
	alert("step 1, ibinstr=" + ibinstr );
	var binstr = deinterleave( ibinstr, 8 );
	alert("step 2, deinterleave=" + binstr );
	var shiftamount = getshiftamount( key, binstr );
	alert("step 3: shiftamount=" + shiftamount + ", key=" + key);
	var unshiftbinstr = strleftshift( binstr, shiftamount );
	alert("dec strleftshift: "+unshiftbinstr);
	var out_str = binstr2str(unshiftbinstr);
	alert("out_str: "+out_str);
	return out_str;
}