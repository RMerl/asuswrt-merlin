function str2binstr(s) {
	var b = "";//new Array();
	var last = s.length;
	for (var i = 0; i < last; i++) {
		var d = s.charCodeAt(i);
		if (d < 128)
			b += dec2Bin(d).toString();
		else {
			var c = s.charAt(i);
			alert(c + ' is NOT an ASCII character');
			//b[i] = -1;
		}
	}
	
	return b;
}

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
	
	//sum hexstring, every 4digit (e.g "495a5c" => 0x495a + 0x5c => 0x49b6 => 18870;
	var sumhex = sumhexstr( seed_hex_str );
	
	//limit shift value to lenght of seed
	var shiftamount = sumhex % orignaldostr.length;

	//ensure there is a shiftamount;
	if( shiftamount == 0 )
		shiftamount = 15;

	return shiftamount;
}

function strrightshift( str, shiftamount )
{
	// e.g strrightshift("abcdef", 2) => "efabcd"
	var aa = str.substring( str.length - shiftamount, str.length );
	var bb = str.substring( 0, str.length - shiftamount );
	return aa.concat(bb);
}
function interleave( src, charcnt )
{ 	
	// example: input string "ABCDEFG", charcnt = 3 --> "ADGBE0CF0"
	var ret = "";
	var ic = Math.ceil( src.length / charcnt );
	for( var i = 0; i < ic; i++ )
	{
		for( var j = 0; j < charcnt; j++ )
		{
			ret += src.charAt( (j * ic) + i );
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

function binstr2hexstr( in_src )
{
	var ret = "";	
	if( 0 != in_src.length % 4 )
	{
		return ret;
	}
			
	var ic = in_src.length / 4;
			
	for( var i = 0; i < ic; i++ )
	{
		var substr = in_src.substr( i*4, 4 );		
		ret += ten2hex( bin2ten( substr ) );
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
	
xt123={}
xt123.encode=function(in_str, key){
	var binstr = str2binstr( in_str );
	alert("binstr:"+binstr);
	var shiftamount = getshiftamount( key, binstr );
	alert("shiftamount:"+shiftamount);
	var shiftbinstr = strrightshift( binstr, shiftamount );
	alert("shiftbinstr:"+shiftbinstr);
	var ishiftbinstr = interleave( shiftbinstr , 8 );
	alert("ishiftbinstr:"+ishiftbinstr);
	return "ASUSHARE" + binstr2hexstr(ishiftbinstr);
}