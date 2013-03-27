class myClass
{
  public:
    myClass() {};
    ~myClass() {};
    void performUnblocking( short int cell_index );
    void performBlocking( int cell_index );
};

void myClass::performUnblocking( short int cell_index ) {}

void myClass::performBlocking( int cell_index ) {}

int main ()
{
  myClass mc;
  mc.performBlocking (0);
  mc.performUnblocking (0);
}

