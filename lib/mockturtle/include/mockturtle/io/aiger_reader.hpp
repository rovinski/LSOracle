/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2019  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file aiger_reader.hpp
  \brief Lorina reader for AIGER files

  \author Mathias Soeken
*/

#pragma once

#include "../networks/aig.hpp"
#include "../traits.hpp"
#include <lorina/aiger.hpp>

namespace mockturtle
{

  template<typename Ntk, typename StorageContainerMap = std::unordered_map<signal<Ntk>, std::vector<std::string>>, typename StorageContainerReverseMap = std::unordered_map<std::string, signal<Ntk>>>
  class NameMap
  {
  public:
    using signal = typename Ntk::signal;

  public:
    NameMap() = default;

    void insert( signal const& s, std::string const& name )
    {
      /* update direct map */
      auto const it = _names.find( s );
      if ( it == _names.end() )
      {
        _names[s] = {name};
      }
      else
      {
        it->second.push_back( name );
      }

      /* update reverse map */
      auto const rev_it = _rev_names.find( name );
      if ( rev_it != _rev_names.end() )
      {
        std::cout << "[w] signal name `" << name << "` is used twice" << std::endl;
      }
      _rev_names.insert( std::make_pair( name, s ) );
    }

    std::vector<std::string> operator[]( signal const& s )
    {
      return _names[s];
    }

    std::vector<std::string> operator[]( signal const& s ) const
    {
      return _names.at( s );
    }

    std::vector<std::string> get_name( signal const& s ) const
    {
      return _names.at( s );
    }

    bool has_name( signal const& s, std::string const& name ) const
    {
      auto const it = _names.find( s );
      if ( it == _names.end() )
      {
        return false;
      }
      return ( std::find( it->second.begin(), it->second.end(), name ) != it->second.end() );
    }

    StorageContainerReverseMap get_name_to_signal_mapping() const
    {
      return _rev_names;
    }

  protected:
    StorageContainerMap _names;
    StorageContainerReverseMap _rev_names;
  }; // NameMap

  /*! \brief Lorina reader callback for Aiger files.
   *
   * **Required network functions:**
   * - `create_pi`
   * - `create_po`
   * - `get_constant`
   * - `create_not`
   * - `create_and`
   *
     \verbatim embed:rst

     Example

     .. code-block:: c++

        aig_network aig;
        lorina::read_aiger( "file.aig", aiger_reader( aig ) );

        mig_network mig;
        lorina::read_aiger( "file.aig", aiger_reader( mig ) );
     \endverbatim
   */
  template<typename Ntk>
  class aiger_reader : public lorina::aiger_reader
  {
  public:
    explicit aiger_reader( Ntk& ntk, NameMap<Ntk>* names = nullptr ) : _ntk( ntk ), _names( names )
    {
      static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
      static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi function" );
      static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po function" );
      static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant function" );
      static_assert( has_create_not_v<Ntk>, "Ntk does not implement the create_not function" );
      static_assert( has_create_and_v<Ntk>, "Ntk does not implement the create_and function" );
    }

    ~aiger_reader()
    {
      uint32_t output_idx = 0u;
      for ( auto out : outputs )
      {
        auto const lit = std::get<0>( out );
        auto signal = signals[lit >> 1];
        if ( lit & 1 )
        {
          signal = _ntk.create_not( signal );
        }
        if ( _names )
          _names->insert( signal, std::get<1>( out ) );
        _ntk.create_po( signal );
        // set default name for POs
        if constexpr ( has_set_output_name_v<Ntk> )
        {
          if ( !_ntk.has_output_name( output_idx ) ){
            std::string name = "po" + std::to_string(output_idx);
            _ntk.set_output_name( output_idx, name );
          }
        }
        output_idx++;
      }
      uint32_t latch_idx = 1u;
      for ( auto latch : latches )
      {
        auto const lit = std::get<0>( latch );
        auto const reset = std::get<1>( latch );

        auto signal = signals[lit >> 1];
        if ( lit & 1 )
        {
          signal = _ntk.create_not( signal );
        }

        if ( _names )
          _names->insert( signal, std::get<2>( latch ) + "_next" );
        _ntk.create_ri( signal, reset );
        // set default name for RIs
        if constexpr ( has_set_output_name_v<Ntk> )
        {
          if ( !_ntk.has_output_name( output_idx ) ){
            std::string name = "li" + std::to_string(latch_idx);
            _ntk.set_output_name( output_idx, name );
          }
        }
        latch_idx++;
        output_idx++;
      }
    }

    void on_header( uint64_t, uint64_t num_inputs, uint64_t num_latches, uint64_t, uint64_t ) const override
    {
      _num_inputs = num_inputs;

      /* constant */
      signals.push_back( _ntk.get_constant( false ) );

      /* create primary inputs (pi) */
      for ( auto i = 0u; i < num_inputs; ++i )
      {
        signals.push_back( _ntk.create_pi() );
        // set default name for PIs
        if constexpr ( has_set_name_v<Ntk> )
        {
          if( !_ntk.has_name(signals.back() )){
            // std::cout << "signal " << signals.back().index << " with name pi" << i << "\n";
            std::string name = "pi" + std::to_string(i);
            _ntk.set_name( signals.back(), name );
          }
        }
      }

      /* create latch outputs (ro) */
      for ( auto i = 0u; i < num_latches; ++i )
      {
        signals.push_back( _ntk.create_ro() );
      }
    }

    void on_input_name( unsigned index, const std::string& name ) const override
    {
      if constexpr ( has_set_name_v<Ntk> )
      {
        _ntk.set_name( signals[1 + index], name );
      }
    }

    void on_output_name( unsigned index, const std::string& name ) const override
    {
      if constexpr ( has_set_output_name_v<Ntk> )
      {
        _ntk.set_output_name( index, name );
      }
    }

    void on_latch_name( unsigned index, const std::string& name ) const override
    {
      if constexpr ( has_set_name_v<Ntk> )
      {
        _ntk.set_name( signals[1 + _num_inputs + index], name );
      }
    }

    void on_and( unsigned index, unsigned left_lit, unsigned right_lit ) const override
    {
      (void)index;
      assert( signals.size() == index );

      auto left = signals[left_lit >> 1];
      if ( left_lit & 1 )
      {
        left = _ntk.create_not( left );
      }

      auto right = signals[right_lit >> 1];
      if ( right_lit & 1 )
      {
        right = _ntk.create_not( right );
      }

      signals.push_back( _ntk.create_and( left, right ) );
    }

    void on_latch( unsigned index, unsigned next, latch_init_value reset ) const override
    {
      (void)index;
      int8_t r = reset == latch_init_value::NONDETERMINISTIC ? -1 : ( reset == latch_init_value::ONE ? 1 : 0 );
      latches.push_back( std::make_tuple( next, r, "" ) );
    }

    void on_output( unsigned index, unsigned lit ) const override
    {
      (void)index;
      assert( index == outputs.size() );
      outputs.emplace_back( lit, "" );
    }

  private:
    Ntk& _ntk;

    mutable uint32_t _num_inputs = 0;
    mutable std::vector<std::tuple<unsigned, std::string>> outputs;
    mutable std::vector<signal<Ntk>> signals;
    mutable std::vector<std::tuple<unsigned, int8_t, std::string>> latches;
    mutable NameMap<Ntk>* _names;
  };

} /* namespace mockturtle */