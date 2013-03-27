
int x;
int y;
int z;
int w;



int main ()
{
   
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif

    x = 14;
    y = 3;
    z = 2;
    w = 2;

    return 0;
    
}

