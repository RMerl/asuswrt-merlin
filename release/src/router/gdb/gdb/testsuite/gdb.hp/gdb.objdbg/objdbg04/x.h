template <class T>
class Adder {
public:
  void set(T);
  T get();
  T add(T);

private:
  T val;
};

template <class T>
void Adder<T>::set(T new_val)
{
  val = new_val;
}

template <class T>
T Adder<T>::get()
{
  return val;
}

template <class T>
T Adder<T>::add(T new_val)
{
  val += new_val;
  return val;
}

