static int file_static = 1234;
int global_from_primary = 5678;

int function_from_primary()
{
    garbage();	
}

force_generation_of_export_stub()
{
    _start();   /* force main module to have an export stub for _start() */
}
