
enum Normal {
  red,
  blue,
  green
};

short enum Small {
  pink,
  cyan,
  grey
};

char enum Tiny {
  orange,
  yellow,
  brown
};


main()
{
  enum Normal normal[3];
  short enum Small small[3];
  char enum Tiny tiny[3];
  int i;

  for (i=0; i < 3; i++)
    {
      normal[i] = (enum Normal) i;
      small[i] = (short enum Small) i;
      tiny[i] = (char enum Tiny) i;
    }
  normal[0] = 0; /* place to hang a breakpoint */ 
}

    
  

  


  
