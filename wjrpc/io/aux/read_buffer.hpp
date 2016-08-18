#pragma once

#include <iow/io/aux/read_buffer_options.hpp>
#include <iow/memory.hpp>
#include <fas/typemanip/empty_type.hpp>
#include <queue>
#include <memory>
#include <algorithm>


namespace iow{ namespace io{


template<typename DataType>
class read_buffer
{
public:
  typedef read_buffer_options<DataType> options_type;
  typedef DataType data_type;
  typedef typename data_type::value_type value_type;
  typedef value_type* value_ptr;
  typedef std::unique_ptr<data_type> data_ptr;
  typedef std::unique_ptr<value_type[]> sep_ptr;
  typedef std::pair<value_ptr, size_t> data_pair;
  typedef std::function<data_ptr(size_t, size_t)> create_fun;
  typedef std::function<void(data_ptr)> free_fun;
private:
  typedef typename data_type::const_iterator const_iterator;
  typedef std::pair<size_t, size_t> search_pair;

public:

  read_buffer()
    : _sep(nullptr)
    , _sep_size(0)
    , _bufsize(4096)
    , _maxbuf(4096*4)
    , _minbuf(512)
    , _trimsep(false)
    , _create(nullptr)
    , _free(nullptr)
    , _size(0)
    , _offset(0)
    , _readbuf(-1)
    , _readpos(-1)
    , _parsebuf(0)
    , _parsepos(0)
  {
    _buffers.reserve(2);
  }

  read_buffer(const read_buffer& other) = delete;
  read_buffer(read_buffer&&) = delete;
  read_buffer& operator=(const read_buffer& other) = delete;
  read_buffer& operator=(read_buffer&& other) = delete;

  template<typename O>
  void set_options(const O& opt) noexcept
  {
    if ( opt.sep.empty() )
    {
      _sep = nullptr;
      _sep_size=0;
    }
    else
    {
      _sep_size = opt.sep.size();
      _sep=sep_ptr(new value_type[_sep_size]);
      std::copy(opt.sep.begin(), opt.sep.end(), _sep.get() );
    }

    _bufsize = opt.bufsize;
    _maxbuf = opt.maxbuf;
    _minbuf = opt.minbuf;
    _trimsep = opt.trimsep;
    _create = opt.create;
    _free = opt.free;

    if ( _bufsize == 0 )
    {
      _bufsize = 4096;
    }
    
    if ( _minbuf==0 )
    {
      _minbuf=128;
    }

    if ( _minbuf > _bufsize )
    {
      _minbuf = _bufsize;
    }

    if ( _maxbuf < _bufsize )
    {
      _maxbuf = _bufsize;
    }
  }

  template<typename O>
  void get_options(O& opt) const noexcept
  {
    if ( _sep_size!=0 )
    {
      opt.sep.assign(_sep.get(), _sep.get() + _sep_size);
    }
    else
    {
      opt.sep.clear();
    }

    opt.bufsize = _bufsize;
    opt.maxbuf  = _maxbuf;
    opt.minbuf  = _minbuf;
    opt.trimsep = _trimsep;
    opt.create  = _create;
    opt.free    = _free;
  }

  void clear() noexcept
  {
    _size = 0;
    _offset = 0;
    _readbuf = -1;
    _readpos = -1;
    _parsebuf = 0;
    _parsepos = 0;
    _buffers.clear();
  }

  size_t count() const noexcept
  {
    return _buffers.size();
  }

  size_t size() const noexcept
  {
    return _size;
  }

  size_t capacity() const noexcept
  {
    size_t result = 0;
    for (auto& buf: _buffers)
      result += _buffers.capacity();
    return result;
  }

  bool waiting() const noexcept
  {
    return _readbuf!=npos();
  }

  data_pair next()
  {
    data_pair result(0,0);

    if ( this->waiting() )
      return result;

    if ( _buffers.empty() )
      return create_for_next_();

    data_ptr& last = _buffers.back();

    size_t reserve = last->capacity() - last->size();
    if ( reserve > _minbuf && _minbuf!=0)
    {
      size_t nextsize = _bufsize < reserve ? _bufsize : reserve;
      _readbuf = _buffers.size() - 1;
      _readpos = last->size();
      last->resize( last->size() + nextsize  );
      result.first = &((*last)[_readpos]);
      result.second = nextsize;
    }
    else
    {
      result = create_for_next_();
    }

    return result;
  }

  bool rollback(data_pair d)
  {
    d.second = 0;
    return this->confirm(d);
  }

  bool confirm(data_pair d)
  {
    if ( !waiting() )
      return false;

    if ( _readbuf >= _buffers.size() )
      return false;

    auto& buf = _buffers[_readbuf];

    if ( d.second > 0 )
    {
      if ( &( (*buf)[0]) + _readpos - d.first != 0 )
        return false;

      if ( buf->size() < _readpos + d.second )
        return false;

      _size += d.second;
    }

    buf->resize( _readpos + d.second );
    if ( buf->empty() )
    {
      free_( std::move(buf) );
      _buffers.erase( _buffers.begin() + _readbuf );
    }
    _readpos = -1;
    _readbuf = -1;

    return true;
  }

  data_ptr detach()
  {
    if ( _buffers.empty() )
      return nullptr;

    auto res = search_();

    if ( res.first == this->npos() )
    {
      if ( _readbuf != this->npos() )
      {
        _parsebuf = _readbuf;
        _parsepos = _readpos;
      }
      else
      {
        _parsebuf = _buffers.size()-1;
        _parsepos = _buffers.back()->size();
      }
      return nullptr;
    }

    auto resbuf = this->make_result_(res);
    this->prepare_(res);
    size_t bufsize = resbuf->size();
    _size -= bufsize;
    if ( _trimsep && bufsize >= _sep_size )
    {
      resbuf->resize( bufsize - _sep_size);
    }

    return std::move(resbuf);
  }

private:

  typedef std::vector<data_ptr> buffer_list;

  constexpr size_t npos() const
  {
    return ~0;
  }

  data_ptr create_(size_t size, size_t maxbuf) const noexcept
  {
    if ( _create!=nullptr )
    {
      return _create(size, maxbuf);
    }

    try
    {
      return std::make_unique<data_type>(size);
    }
    catch(const std::bad_alloc& )
    {
      return nullptr;
    }
  }

  data_ptr create_(size_t size) const noexcept
  {
    return this->create_(size, _bufsize);
  }

  data_ptr create_() const noexcept
  {
    return this->create_(_bufsize);
  }
  
  void free_(data_ptr d) const
  {
    if ( _free != nullptr)
      _free( std::move(d) );
  }

private:

  /**************************************************************************/
  /***************************** next helper ******************************/
  /**************************************************************************/

  data_pair create_for_next_()
  {
    auto ptr = create_();
    
    if ( ptr==nullptr )
      return data_pair(nullptr, -1);

    // Если закончили парсить на последнем буфере
    if ( !_buffers.empty() 
          && _parsebuf == _buffers.size() - 1
          && _buffers[_parsebuf]->size() == _parsepos )
    {
      _parsebuf = _buffers.size();
      _parsepos = 0;
    }

    _buffers.push_back( std::move(ptr) );
    _readbuf = _buffers.size()-1;
    _readpos = 0;
    data_ptr& last = _buffers.back();
    return data_pair( &((*last)[0]), last->size());
  }

  /**************************************************************************/
  /***************************** detach helper ******************************/
  /**************************************************************************/

  size_t last_buff_() const
  {
    if ( _readbuf==-1 || (_readpos > 0 && _readpos!=-1) )
    {
      return _buffers.size() - 1;
    }
    else if (_buffers.size() > 1 )
    {
      return _buffers.size() - 2;
    }
    return -1;
  }

  const_iterator begin_(size_t pos) const
  {
    const_iterator itr = _buffers[pos]->begin();
    if ( pos == 0 )
      itr += _offset;
    return itr;
  }

  const_iterator end_(size_t pos) const
  {
    if ( pos == _readbuf )
    {
      return _buffers[pos]->begin() + _readpos;
    }
    return _buffers[pos]->end();
  }

  const_iterator last_(size_t pos) const
  {
    auto& buf = *(_buffers[pos]);
    if ( pos == _readbuf )
    {
      if (_readpos==0)
      {
        abort();
      }
      return buf.begin() + _readpos - 1;
    }

    if ( buf.empty() )
    {
      abort();
    }
    return buf.begin() + buf.size() - 1;
  }

  void dec_(size_t& pos, const_iterator& itr) const
  {
    auto beg = begin_(pos);
    if ( beg != itr )
    {
      --itr;
    }
    else
    {
      if ( pos == 0 )
      {
        pos = -1;
      }
      else
      {
        --pos;
        itr = last_(pos);
      }
    }
  }

  bool check_sep_( size_t pos, const_iterator itr) const
  {
    if ( _sep_size < 2)
      return true;

    dec_(pos, itr);
    if ( pos == npos() )
      return false;

    if ( _sep_size == 2 )
      return *itr == _sep[0];

    value_type* sbeg = _sep.get();
    value_type* scur = sbeg + _sep_size - 2;
    if ( *itr != *scur )
    {
      return false;
    }
    else if (_sep_size == 2)
    {
      return true;
    }

    for(;;)
    {
      --scur;
      dec_(pos, itr);
      if ( pos == npos() )
        return false;
      if ( *itr != *scur )
        return false;
      if (scur==sbeg)
        return true;
    }
  }

  search_pair nosep_search_() const
  {
    if ( _offset!=0 && _parsebuf==0 && _offset==_parsepos ) 
    {
      return search_pair(-1, -1);
    }

    if ( _readbuf==npos() )
    {
      // Если последний буфер не выделен под чтение
      return search_pair(_buffers.size() - 1, _buffers.back()->size());
    }
    else if (_readpos != 0)
    {
      // Если последний буфер выделен под чтение, но не сначала
      return search_pair(_readbuf, _readpos);
    }
    else if ( _readbuf != 0 )
    {
      // Если последний буфер выделен под чтение с начала
      // и он не первый
      return search_pair( _readbuf-1, _buffers[_readbuf-1]->size());
    }
    // Нет готовых данных
    return search_pair(-1, -1);
  }

  search_pair search_() const
  {
    if ( _buffers.empty() || _parsebuf==npos() )
      return search_pair(-1, -1);

    if (_sep_size==0)
      return nosep_search_();

    // Если ожидаем confirm() для парсинга
    if (_readbuf==_parsebuf && _readpos==_parsepos)
      return search_pair(-1, -1);

    // Если нужен next() confirm() для парсинга
    if (_buffers[_parsebuf]->size() == _parsepos )
      return search_pair(-1, -1);

    // Если последний буфер выделен полностью для чтения, то игнорируем его
    size_t toparse = _buffers.size() - (_readbuf!=npos() && _readpos==0);

    for (size_t i=_parsebuf; i < toparse; ++i)
    {
      const_iterator end = end_(i);
      const_iterator beg;
      if ( i==_parsebuf )
      {
        beg = _buffers[i]->begin() + _parsepos;
      }
      else
      {
        beg = begin_(i);
      }

      while ( beg!=end )
      {
        beg = std::find(beg, end, _sep[_sep_size-1]);

        if ( beg == end )
          break;

        if ( check_sep_(i, beg) )
        {
          return search_pair( i, std::distance(_buffers[i]->cbegin(), beg) + 1 );
        }
        ++beg;
      }
    }
    return search_pair(-1, -1);
  }

  // Удаляем отработанные буферы и настраиваем состояние
  void prepare_(const search_pair& p) 
  {
//#warning TODO: сократить список буфферов при разрастании 
    if ( p.first==0 )
    {
      if ( _buffers[0] == nullptr )
      {
        _parsepos = 0;
        _parsebuf = 0;
        free_(std::move( _buffers.front() ) );
        _buffers.erase( _buffers.begin() );
        _offset = 0;

        if (_readbuf != npos())
        {
          if (_readbuf==0)
            abort();
          --_readbuf;
        }
      }
      else
      {
        _offset = p.second;
        _parsepos = p.second;
        _parsebuf = 0;
      }
    }
    else
    {
      bool complete = _buffers[p.first]->size() == p.second;
      size_t off = p.first + complete;
      if ( off > 0 )
      {
        std::for_each(_buffers.begin(), _buffers.begin() + off, [this](data_ptr& d){ this->free_( std::move(d) );});
        _buffers.erase( _buffers.begin(), _buffers.begin() + off );
        if (_readbuf != npos())
        {
          if (_readbuf < off)
            abort();
          _readbuf-=off;
        }
      }
      _parsebuf = 0;
      _offset = complete ? 0 : p.second;
      _parsepos = _offset;
    }
    
    if ( _buffers.size() > 128 &&  _buffers.size()*2 < _buffers.capacity() )
      _buffers.shrink_to_fit();

  }

  data_ptr make_result_(const search_pair& p)
  {
    data_ptr result = make_result_if_first_(p);

    // Если блок готов в первом буфере
    if ( result != nullptr )
      return std::move(result);

    // Расчитываем необходимый резерв
    size_t reserve = 0;
    for (size_t i=0; i < p.first + 1; ++i)
    {
      reserve+=_buffers[i]->size();
    }

    // reserve с небольшим оверхедом, поэтому очищаем и используем inserter
    result = create_(reserve, reserve*2 < _maxbuf ? reserve*2 : reserve);
    result->clear();

    // Копируем со всех буферов, что готовы
    for (size_t i=0; i < p.first + 1; ++i)
    {
      if ( i != p.first )
      {
        std::copy(begin_(i), end_(i), std::inserter(*result, result->end()));
      }
      else
      {
        std::copy(begin_(i), _buffers[i]->cbegin() + p.second, std::inserter(*result, result->end()));
      }
    }

    return std::move(result);
  }

  data_ptr make_result_if_first_(const search_pair& p)
  {
    if ( p.first!=0 )
      return nullptr;

    data_ptr result = nullptr;

    // Если можем полностью захватить буфер
     size_t tmp = _buffers[0]->size();
    if ( tmp == p.second )
    {
      result = std::move(_buffers[0]);
      // Если ральные данные не с начала буфера
      if (_offset!=0)
      {
        result->erase(result->begin(), result->begin() + _offset);
      }
    }
    else
    {
      size_t size = p.second - _offset;
      result = create_(p.second - _offset, size*2 < _maxbuf ? size*2 : size);
      std::copy(_buffers[0]->begin() + _offset, _buffers[0]->begin() + p.second, result->begin() );
    }
    return std::move(result);
  }

private:

// options
  sep_ptr _sep;
  size_t _sep_size;
  size_t _bufsize;
  size_t _maxbuf;
  size_t _minbuf;
  bool   _trimsep;

  create_fun _create;
  free_fun _free;

  size_t  _size;
  size_t  _offset;  // Смещение в первом буфере
  size_t  _readbuf; // -1 - если не ожидает подтверждения
  size_t  _readpos;
  
  // Номер буфера, с которого продолжить парсинг
  //   при _buffers.empty() равен 0, но в это случае поиск не производится
  size_t  _parsebuf;
  // Позиция в буфере, с которого продолжить парсинг
  // Может быть равен _buffers[_parsebuf]->size() для _buffers.size()-1==_parsebuf
  //   в этом случае корректируется при следующем next(), если выделяеться новый буфер
  //   и поиск до confirm() недоступен
  size_t  _parsepos;

  buffer_list   _buffers;
};

}}
