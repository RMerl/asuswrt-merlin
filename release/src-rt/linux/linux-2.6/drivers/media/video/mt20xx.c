/*
 *
 * i2c tv tuner chip device driver
 * controls microtune tuners, mt2032 + mt2050 at the moment.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/videodev.h>
#include <linux/moduleparam.h>
#include <media/tuner.h>

/* ---------------------------------------------------------------------- */

static unsigned int optimize_vco  = 1;
module_param(optimize_vco,      int, 0644);

static unsigned int tv_antenna    = 1;
module_param(tv_antenna,        int, 0644);

static unsigned int radio_antenna = 0;
module_param(radio_antenna,     int, 0644);

/* from tuner-core.c */
extern int tuner_debug;

/* ---------------------------------------------------------------------- */

#define MT2032 0x04
#define MT2030 0x06
#define MT2040 0x07
#define MT2050 0x42

static char *microtune_part[] = {
	[ MT2030 ] = "MT2030",
	[ MT2032 ] = "MT2032",
	[ MT2040 ] = "MT2040",
	[ MT2050 ] = "MT2050",
};

// IsSpurInBand()?
static int mt2032_spurcheck(struct i2c_client *c,
			    int f1, int f2, int spectrum_from,int spectrum_to)
{
	struct tuner *t = i2c_get_clientdata(c);
	int n1=1,n2,f;

	f1=f1/1000; //scale to kHz to avoid 32bit overflows
	f2=f2/1000;
	spectrum_from/=1000;
	spectrum_to/=1000;

	tuner_dbg("spurcheck f1=%d f2=%d  from=%d to=%d\n",
		  f1,f2,spectrum_from,spectrum_to);

	do {
	    n2=-n1;
	    f=n1*(f1-f2);
	    do {
		n2--;
		f=f-f2;
		tuner_dbg("spurtest n1=%d n2=%d ftest=%d\n",n1,n2,f);

		if( (f>spectrum_from) && (f<spectrum_to))
			tuner_dbg("mt2032 spurcheck triggered: %d\n",n1);
	    } while ( (f>(f2-spectrum_to)) || (n2>-5));
	    n1++;
	} while (n1<5);

	return 1;
}

static int mt2032_compute_freq(struct i2c_client *c,
			       unsigned int rfin,
			       unsigned int if1, unsigned int if2,
			       unsigned int spectrum_from,
			       unsigned int spectrum_to,
			       unsigned char *buf,
			       int *ret_sel,
			       unsigned int xogc) //all in Hz
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned int fref,lo1,lo1n,lo1a,s,sel,lo1freq, desired_lo1,
		desired_lo2,lo2,lo2n,lo2a,lo2num,lo2freq;

	fref= 5250 *1000; //5.25MHz
	desired_lo1=rfin+if1;

	lo1=(2*(desired_lo1/1000)+(fref/1000)) / (2*fref/1000);
	lo1n=lo1/8;
	lo1a=lo1-(lo1n*8);

	s=rfin/1000/1000+1090;

	if(optimize_vco) {
		if(s>1890) sel=0;
		else if(s>1720) sel=1;
		else if(s>1530) sel=2;
		else if(s>1370) sel=3;
		else sel=4; // >1090
	}
	else {
		if(s>1790) sel=0; // <1958
		else if(s>1617) sel=1;
		else if(s>1449) sel=2;
		else if(s>1291) sel=3;
		else sel=4; // >1090
	}
	*ret_sel=sel;

	lo1freq=(lo1a+8*lo1n)*fref;

	tuner_dbg("mt2032: rfin=%d lo1=%d lo1n=%d lo1a=%d sel=%d, lo1freq=%d\n",
		  rfin,lo1,lo1n,lo1a,sel,lo1freq);

	desired_lo2=lo1freq-rfin-if2;
	lo2=(desired_lo2)/fref;
	lo2n=lo2/8;
	lo2a=lo2-(lo2n*8);
	lo2num=((desired_lo2/1000)%(fref/1000))* 3780/(fref/1000); //scale to fit in 32bit arith
	lo2freq=(lo2a+8*lo2n)*fref + lo2num*(fref/1000)/3780*1000;

	tuner_dbg("mt2032: rfin=%d lo2=%d lo2n=%d lo2a=%d num=%d lo2freq=%d\n",
		  rfin,lo2,lo2n,lo2a,lo2num,lo2freq);

	if(lo1a<0 || lo1a>7 || lo1n<17 ||lo1n>48 || lo2a<0 ||lo2a >7 ||lo2n<17 || lo2n>30) {
		tuner_info("mt2032: frequency parameters out of range: %d %d %d %d\n",
			   lo1a, lo1n, lo2a,lo2n);
		return(-1);
	}

	mt2032_spurcheck(c, lo1freq, desired_lo2,  spectrum_from, spectrum_to);
	// should recalculate lo1 (one step up/down)

	// set up MT2032 register map for transfer over i2c
	buf[0]=lo1n-1;
	buf[1]=lo1a | (sel<<4);
	buf[2]=0x86; // LOGC
	buf[3]=0x0f; //reserved
	buf[4]=0x1f;
	buf[5]=(lo2n-1) | (lo2a<<5);
	if(rfin >400*1000*1000)
		buf[6]=0xe4;
	else
		buf[6]=0xf4; // set PKEN per rev 1.2
	buf[7]=8+xogc;
	buf[8]=0xc3; //reserved
	buf[9]=0x4e; //reserved
	buf[10]=0xec; //reserved
	buf[11]=(lo2num&0xff);
	buf[12]=(lo2num>>8) |0x80; // Lo2RST

	return 0;
}

static int mt2032_check_lo_lock(struct i2c_client *c)
{
	struct tuner *t = i2c_get_clientdata(c);
	int try,lock=0;
	unsigned char buf[2];

	for(try=0;try<10;try++) {
		buf[0]=0x0e;
		i2c_master_send(c,buf,1);
		i2c_master_recv(c,buf,1);
		tuner_dbg("mt2032 Reg.E=0x%02x\n",buf[0]);
		lock=buf[0] &0x06;

		if (lock==6)
			break;

		tuner_dbg("mt2032: pll wait 1ms for lock (0x%2x)\n",buf[0]);
		udelay(1000);
	}
	return lock;
}

static int mt2032_optimize_vco(struct i2c_client *c,int sel,int lock)
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned char buf[2];
	int tad1;

	buf[0]=0x0f;
	i2c_master_send(c,buf,1);
	i2c_master_recv(c,buf,1);
	tuner_dbg("mt2032 Reg.F=0x%02x\n",buf[0]);
	tad1=buf[0]&0x07;

	if(tad1 ==0) return lock;
	if(tad1 ==1) return lock;

	if(tad1==2) {
		if(sel==0)
			return lock;
		else sel--;
	}
	else {
		if(sel<4)
			sel++;
		else
			return lock;
	}

	tuner_dbg("mt2032 optimize_vco: sel=%d\n",sel);

	buf[0]=0x0f;
	buf[1]=sel;
	i2c_master_send(c,buf,2);
	lock=mt2032_check_lo_lock(c);
	return lock;
}


static void mt2032_set_if_freq(struct i2c_client *c, unsigned int rfin,
			       unsigned int if1, unsigned int if2,
			       unsigned int from, unsigned int to)
{
	unsigned char buf[21];
	int lint_try,ret,sel,lock=0;
	struct tuner *t = i2c_get_clientdata(c);

	tuner_dbg("mt2032_set_if_freq rfin=%d if1=%d if2=%d from=%d to=%d\n",
		  rfin,if1,if2,from,to);

	buf[0]=0;
	ret=i2c_master_send(c,buf,1);
	i2c_master_recv(c,buf,21);

	buf[0]=0;
	ret=mt2032_compute_freq(c,rfin,if1,if2,from,to,&buf[1],&sel,t->xogc);
	if (ret<0)
		return;

	// send only the relevant registers per Rev. 1.2
	buf[0]=0;
	ret=i2c_master_send(c,buf,4);
	buf[5]=5;
	ret=i2c_master_send(c,buf+5,4);
	buf[11]=11;
	ret=i2c_master_send(c,buf+11,3);
	if(ret!=3)
		tuner_warn("i2c i/o error: rc == %d (should be 3)\n",ret);

	// wait for PLLs to lock (per manual), retry LINT if not.
	for(lint_try=0; lint_try<2; lint_try++) {
		lock=mt2032_check_lo_lock(c);

		if(optimize_vco)
			lock=mt2032_optimize_vco(c,sel,lock);
		if(lock==6) break;

		tuner_dbg("mt2032: re-init PLLs by LINT\n");
		buf[0]=7;
		buf[1]=0x80 +8+t->xogc; // set LINT to re-init PLLs
		i2c_master_send(c,buf,2);
		mdelay(10);
		buf[1]=8+t->xogc;
		i2c_master_send(c,buf,2);
	}

	if (lock!=6)
		tuner_warn("MT2032 Fatal Error: PLLs didn't lock.\n");

	buf[0]=2;
	buf[1]=0x20; // LOGC for optimal phase noise
	ret=i2c_master_send(c,buf,2);
	if (ret!=2)
		tuner_warn("i2c i/o error: rc == %d (should be 2)\n",ret);
}


static void mt2032_set_tv_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = i2c_get_clientdata(c);
	int if2,from,to;

	// signal bandwidth and picture carrier
	if (t->std & V4L2_STD_525_60) {
		// NTSC
		from = 40750*1000;
		to   = 46750*1000;
		if2  = 45750*1000;
	} else {
		// PAL
		from = 32900*1000;
		to   = 39900*1000;
		if2  = 38900*1000;
	}

	mt2032_set_if_freq(c, freq*62500 /* freq*1000*1000/16 */,
			   1090*1000*1000, if2, from, to);
}

static void mt2032_set_radio_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = i2c_get_clientdata(c);
	int if2 = t->radio_if2;

	// per Manual for FM tuning: first if center freq. 1085 MHz
	mt2032_set_if_freq(c, freq * 1000 / 16,
			      1085*1000*1000,if2,if2,if2);
}

// Initalization as described in "MT203x Programming Procedures", Rev 1.2, Feb.2001
static int mt2032_init(struct i2c_client *c)
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned char buf[21];
	int ret,xogc,xok=0;

	// Initialize Registers per spec.
	buf[1]=2; // Index to register 2
	buf[2]=0xff;
	buf[3]=0x0f;
	buf[4]=0x1f;
	ret=i2c_master_send(c,buf+1,4);

	buf[5]=6; // Index register 6
	buf[6]=0xe4;
	buf[7]=0x8f;
	buf[8]=0xc3;
	buf[9]=0x4e;
	buf[10]=0xec;
	ret=i2c_master_send(c,buf+5,6);

	buf[12]=13;  // Index register 13
	buf[13]=0x32;
	ret=i2c_master_send(c,buf+12,2);

	// Adjust XOGC (register 7), wait for XOK
	xogc=7;
	do {
		tuner_dbg("mt2032: xogc = 0x%02x\n",xogc&0x07);
		mdelay(10);
		buf[0]=0x0e;
		i2c_master_send(c,buf,1);
		i2c_master_recv(c,buf,1);
		xok=buf[0]&0x01;
		tuner_dbg("mt2032: xok = 0x%02x\n",xok);
		if (xok == 1) break;

		xogc--;
		tuner_dbg("mt2032: xogc = 0x%02x\n",xogc&0x07);
		if (xogc == 3) {
			xogc=4; // min. 4 per spec
			break;
		}
		buf[0]=0x07;
		buf[1]=0x88 + xogc;
		ret=i2c_master_send(c,buf,2);
		if (ret!=2)
			tuner_warn("i2c i/o error: rc == %d (should be 2)\n",ret);
	} while (xok != 1 );
	t->xogc=xogc;

	t->set_tv_freq    = mt2032_set_tv_freq;
	t->set_radio_freq = mt2032_set_radio_freq;
	return(1);
}

static void mt2050_set_antenna(struct i2c_client *c, unsigned char antenna)
{
	struct tuner *t = i2c_get_clientdata(c);
       unsigned char buf[2];
       int ret;

       buf[0] = 6;
       buf[1] = antenna ? 0x11 : 0x10;
       ret=i2c_master_send(c,buf,2);
       tuner_dbg("mt2050: enabled antenna connector %d\n", antenna);
}

static void mt2050_set_if_freq(struct i2c_client *c,unsigned int freq, unsigned int if2)
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned int if1=1218*1000*1000;
	unsigned int f_lo1,f_lo2,lo1,lo2,f_lo1_modulo,f_lo2_modulo,num1,num2,div1a,div1b,div2a,div2b;
	int ret;
	unsigned char buf[6];

	tuner_dbg("mt2050_set_if_freq freq=%d if1=%d if2=%d\n",
		  freq,if1,if2);

	f_lo1=freq+if1;
	f_lo1=(f_lo1/1000000)*1000000;

	f_lo2=f_lo1-freq-if2;
	f_lo2=(f_lo2/50000)*50000;

	lo1=f_lo1/4000000;
	lo2=f_lo2/4000000;

	f_lo1_modulo= f_lo1-(lo1*4000000);
	f_lo2_modulo= f_lo2-(lo2*4000000);

	num1=4*f_lo1_modulo/4000000;
	num2=4096*(f_lo2_modulo/1000)/4000;

	// todo spurchecks

	div1a=(lo1/12)-1;
	div1b=lo1-(div1a+1)*12;

	div2a=(lo2/8)-1;
	div2b=lo2-(div2a+1)*8;

	if (tuner_debug > 1) {
		tuner_dbg("lo1 lo2 = %d %d\n", lo1, lo2);
		tuner_dbg("num1 num2 div1a div1b div2a div2b= %x %x %x %x %x %x\n",
			  num1,num2,div1a,div1b,div2a,div2b);
	}

	buf[0]=1;
	buf[1]= 4*div1b + num1;
	if(freq<275*1000*1000) buf[1] = buf[1]|0x80;

	buf[2]=div1a;
	buf[3]=32*div2b + num2/256;
	buf[4]=num2-(num2/256)*256;
	buf[5]=div2a;
	if(num2!=0) buf[5]=buf[5]|0x40;

	if (tuner_debug > 1) {
		int i;
		tuner_dbg("bufs is: ");
		for(i=0;i<6;i++)
			printk("%x ",buf[i]);
		printk("\n");
	}

	ret=i2c_master_send(c,buf,6);
	if (ret!=6)
		tuner_warn("i2c i/o error: rc == %d (should be 6)\n",ret);
}

static void mt2050_set_tv_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned int if2;

	if (t->std & V4L2_STD_525_60) {
		// NTSC
		if2 = 45750*1000;
	} else {
		// PAL
		if2 = 38900*1000;
	}
	if (V4L2_TUNER_DIGITAL_TV == t->mode) {
		// DVB (pinnacle 300i)
		if2 = 36150*1000;
	}
	mt2050_set_if_freq(c, freq*62500, if2);
	mt2050_set_antenna(c, tv_antenna);
}

static void mt2050_set_radio_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = i2c_get_clientdata(c);
	int if2 = t->radio_if2;

	mt2050_set_if_freq(c, freq * 1000 / 16, if2);
	mt2050_set_antenna(c, radio_antenna);
}

static int mt2050_init(struct i2c_client *c)
{
	struct tuner *t = i2c_get_clientdata(c);
	unsigned char buf[2];
	int ret;

	buf[0]=6;
	buf[1]=0x10;
	ret=i2c_master_send(c,buf,2); //  power

	buf[0]=0x0f;
	buf[1]=0x0f;
	ret=i2c_master_send(c,buf,2); // m1lo

	buf[0]=0x0d;
	ret=i2c_master_send(c,buf,1);
	i2c_master_recv(c,buf,1);

	tuner_dbg("mt2050: sro is %x\n",buf[0]);
	t->set_tv_freq    = mt2050_set_tv_freq;
	t->set_radio_freq = mt2050_set_radio_freq;
	return 0;
}

int microtune_init(struct i2c_client *c)
{
	struct tuner *t = i2c_get_clientdata(c);
	char *name;
	unsigned char buf[21];
	int company_code;

	memset(buf,0,sizeof(buf));
	t->set_tv_freq    = NULL;
	t->set_radio_freq = NULL;
	t->standby    = NULL;
	if (t->std & V4L2_STD_525_60) {
		tuner_dbg("pinnacle ntsc\n");
		t->radio_if2 = 41300 * 1000;
	} else {
		tuner_dbg("pinnacle pal\n");
		t->radio_if2 = 33300 * 1000;
	}
	name = "unknown";

	i2c_master_send(c,buf,1);
	i2c_master_recv(c,buf,21);
	if (tuner_debug) {
		int i;
		tuner_dbg("MT20xx hexdump:");
		for(i=0;i<21;i++) {
			printk(" %02x",buf[i]);
			if(((i+1)%8)==0) printk(" ");
		}
		printk("\n");
	}
	company_code = buf[0x11] << 8 | buf[0x12];
	tuner_info("microtune: companycode=%04x part=%02x rev=%02x\n",
		   company_code,buf[0x13],buf[0x14]);


	if (buf[0x13] < ARRAY_SIZE(microtune_part) &&
	    NULL != microtune_part[buf[0x13]])
		name = microtune_part[buf[0x13]];
	switch (buf[0x13]) {
	case MT2032:
		mt2032_init(c);
		break;
	case MT2050:
		mt2050_init(c);
		break;
	default:
		tuner_info("microtune %s found, not (yet?) supported, sorry :-/\n",
			   name);
		return 0;
	}

	strlcpy(c->name, name, sizeof(c->name));
	tuner_info("microtune %s found, OK\n",name);
	return 0;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
