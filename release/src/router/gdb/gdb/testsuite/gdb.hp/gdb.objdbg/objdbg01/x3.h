
template <class T>
class Info {
public:
  void p(T *x);
};

template <class T>
void Info<T>::p(T *x)
{
  x->print();
}

class PP {
public:
  void print();
};

class QQ {
public:
  void print();
};
