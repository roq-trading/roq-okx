.. _roq-okx:

.. |dagger| unicode:: U+2020
.. |double-dagger| unicode:: U+2021
.. |right-arrow| unicode:: U+2192
.. |right-double-arrow| unicode:: U+21D2
.. |left-right-double-arrow| unicode:: U+21D4
.. |check-mark| unicode:: U+2705
.. |cross-mark| unicode:: U+274C
.. |negative-cross-mark| unicode:: U+274E
.. |footnote-1| unicode:: U+2776
.. |footnote-2| unicode:: U+2777
.. |footnote-3| unicode:: U+2778


roq-okx
=======

.. important::
   The account must be configured for the "net" position mode (futures and swaps).

.. tab:: Unstable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/unstable \
           roq-okx

.. tab:: Stable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/stable \
           roq-okx


Supports
~~~~~~~~

.. grid::  2
  :gutter: 2

  .. grid-item-card::  Products

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:enumerator:`Spot <roq::SecurityType::SPOT>`
        - |check-mark|
        -
      * - :cpp:enumerator:`Futures <roq::SecurityType::FUTURES>`
        - |check-mark|
        -
      * - :cpp:enumerator:`Swap <roq::SecurityType::SWAP>`
        - |check-mark|
        -
      * - :cpp:enumerator:`Option <roq::SecurityType::OPTION>`
        - |cross-mark|
        -

  .. grid-item-card::  Market Data

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`ReferenceData <roq::ReferenceData>`
        - |check-mark|
        -
      * - :cpp:class:`MarketStatus <roq::MarketStatus>`
        - |check-mark|
        -
      * - :cpp:class:`TopOfBook <roq::TopOfBook>`
        - |check-mark|
        -
      * - :cpp:class:`MarketByPrice <roq::MarketByPriceUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`MarketByOrder <roq::MarketByOrderUpdate>`
        - |cross-mark|
        -
      * - :cpp:class:`TradeSummary <roq::TradeSummary>`
        - |check-mark|
        -
      * - :cpp:class:`Statistics <roq::StatisticsUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`TimeSeries <roq::TimeSeriesUpdate>`
        - |check-mark|
        -

  .. grid-item-card::  Orders & Quotes

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`CreateOrder <roq::CreateOrder>`
        - |check-mark|
        -
      * - :cpp:class:`ModifyOrder <roq::ModifyOrder>`
        - |check-mark|
        -
      * - :cpp:class:`CancelOrder <roq::CancelOrder>`
        - |check-mark|
        -
      * - :cpp:class:`CancelAllOrders <roq::CancelAllOrders>`
        - |check-mark|
        -
      * - :cpp:class:`MassQuote <roq::MassQuote>`
        - |cross-mark|
        -
      * - :cpp:class:`CancelQuotes <roq::CancelQuotes>`
        - |cross-mark|
        -

  .. grid-item-card::  Account

    .. list-table::
      :widths: auto
      :align: left

      * - :cpp:class:`Funds <roq::FundsUpdate>`
        - |check-mark|
        -
      * - :cpp:class:`Position <roq::PositionUpdate>`
        - |check-mark|
        -

.. note::

   |check-mark| = Available.

   |negative-cross-mark| = Not implemented.

   |cross-mark| = Unavailable.


Using
-----

.. code-block:: shell

   $ roq-okx [FLAGS]


.. _roq-okx-flags:

Flags
~~~~~

.. code-block:: shell

   $ roq-okx --help

.. tab:: Flags

   .. include:: flags/flags.rstinc

.. tab:: REST

   .. include:: flags/rest.rstinc

.. tab:: WS

   .. include:: flags/ws.rstinc

.. tab:: MBP

   .. include:: flags/mbp.rstinc

.. tab:: Request

   .. include:: flags/request.rstinc

.. tab:: Misc

   .. include:: flags/misc.rstinc


Environments
~~~~~~~~~~~~

.. tab:: Prod

   .. code-block:: shell

      $ --flagfile $CONDA_PREFIX/share/roq-okx/flags/prod/flags.cfg

   .. include:: flags/prod/flags.cfg
     :code: ini

.. tab:: Test

   .. code-block:: shell

      $ --flagfile $CONDA_PREFIX/share/roq-okx/flags/test/flags.cfg

   .. include:: flags/test/flags.cfg
     :code: ini


Configuration
~~~~~~~~~~~~~

.. code-block:: shell

   $ --flagfile $CONDA_PREFIX/share/roq-okx/config.toml

.. important::

   This template will be replaced when the software is upgraded.
   Make a copy and modify to your own needs.

.. include:: config.toml
   :code: toml


Market Data
~~~~~~~~~~~


Inbound
~~~~~~~

.. tab:: TradingStatus

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - :code:`state`
       -
       -

     * - :code:`live`
       - |right-double-arrow|
       - :cpp:enumerator:`OPEN <roq::TradingStatus::OPEN>`

     * - :code:`suspended`
       - |right-double-arrow|
       - :cpp:enumerator:`HALT <roq::TradingStatus::HALT>`

     * - :code:`preopen`
       - |right-double-arrow|
       - :cpp:enumerator:`PRE_OPEN <roq::TradingStatus::PRE_OPEN>`

     * - :code:`settlement`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::TradingStatus::UNDEFINED>`

     * - :code:`expired`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::TradingStatus::UNDEFINED>`

     * - :code:`test`
       - |right-double-arrow|
       - :cpp:enumerator:`UNDEFINED <roq::TradingStatus::UNDEFINED>`


.. tab:: StatisticsType

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - Event
       - Field
       -
       -

     * - :code:`tickers`
       - :code:`open24h`
       - |right-double-arrow|
       - :cpp:enumerator:`OPEN_PRICE <roq::StatisticsType::OPEN_PRICE>`

     * - :code:`tickers`
       - :code:`high24h`
       - |right-double-arrow|
       - :cpp:enumerator:`HIGHEST_TRADED_PRICE <roq::StatisticsType::HIGHEST_TRADED_PRICE>`

     * - :code:`tickers`
       - :code:`low24h`
       - |right-double-arrow|
       - :cpp:enumerator:`LOWEST_TRADED_PRICE <roq::StatisticsType::LOWEST_TRADED_PRICE>`

     * - :code:`tickers`
       - :code:`vol24h`
       - |right-double-arrow|
       - :cpp:enumerator:`TRADE_VOLUME <roq::StatisticsType::TRADE_VOLUME>`

     * - :code:`index-tickers`
       - :code:`idxPx`
       - |right-double-arrow|
       - :cpp:enumerator:`INDEX_VALUE <roq::StatisticsType::INDEX_VALUE>`

     * - :code:`funding-rate`
       - :code:`fundingRate`
       - |right-double-arrow|
       - :cpp:enumerator:`FUNDING_RATE <roq::StatisticsType::FUNDING_RATE>`

     * - :code:`funding-rate`
       - :code:`nextFundingRate`
       - |right-double-arrow|
       - :cpp:enumerator:`FUNDING_RATE_PREDICTION <roq::StatisticsType::FUNDING_RATE_PREDICTION>`



  * - :cpp:class:`LIMIT`
    - :cpp:class:`GTC`
    -
    - →
    - :code:`limit`
    - :code:`false`

  * - :cpp:class:`LIMIT`
    - :cpp:class:`GTC`
    - :cpp:class:`DO_NOT_INCREASE`
    - →
    - :code:`limit`
    - :code:`true`

  * - :cpp:class:`LIMIT`
    - :cpp:class:`GTC`
    - :cpp:class:`PARTICIPATE_DO_NOT_INITIATE`
    - →
    - :code:`post_only`
    - :code:`false`

  * - :cpp:class:`LIMIT`
    - :cpp:class:`FOK`
    -
    - →
    - :code:`fok`
    - :code:`false`

  * - :cpp:class:`LIMIT`
    - :cpp:class:`IOC`
    -
    - →
    - :code:`ioc`
    - :code:`false`

Outbound
~~~~~~~~

.. tab:: CreateOrder

   .. list-table::
     :header-rows: 1
     :widths: auto
     :align: left

     * - :cpp:member:`order_type <roq::CreateOrder::order_type>`
       - :cpp:member:`time_in_force <roq::CreateOrder::time_in_force>`
       - :cpp:member:`execution_instructions <roq::CreateOrder::execution_instructions>`
       -
       - :code:`type`
       - :code:`reduceOnly`

     * - :cpp:enumerator:`MARKET <roq::OrderType::MARKET>`
       -
       -
       - |right-double-arrow|
       - :code:`market`
       - :code:`false`

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`GTC <roq::TimeInForce::GTC>`
       -
       - |right-double-arrow|
       - :code:`limit`
       - :code:`false`

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`GTC <roq::TimeInForce::GTC>`
       - :cpp:enumerator:`DO_NOT_INCREASE <roq::ExecutionInstruction::DO_NOT_INCREASE>`
       - |right-double-arrow|
       - :code:`limit`
       - :code:`true`

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`GTC <roq::TimeInForce::GTC>`
       - :cpp:enumerator:`PARTICIPATE_DO_NOT_INITIATE <roq::ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE>`
       - |right-double-arrow|
       - :code:`post_only`
       - :code:`false`

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`FOK <roq::TimeInForce::FOK>`
       -
       - |right-double-arrow|
       - :code:`fok`
       - :code:`false`

     * - :cpp:enumerator:`LIMIT <roq::OrderType::LIMIT>`
       - :cpp:enumerator:`IOC <roq::TimeInForce::IOC>`
       -
       - |right-double-arrow|
       - :code:`ioc`
       - :code:`false`


.. tab:: ModifyOrder

   TBD


.. tab:: CancelOrder

   TBD


.. tab:: CancelAllOrders

   TBD




Comments
~~~~~~~~

* Only VIP members can access the L2 tick-by-tick market data feed.

* TopOfBook is throttled at 100ms (by exchange)

* The :code:`index-tickers` and :code:`funding-rate` channels are subscribed for
  all swaps and futures.
  This may generate some false subscriptions caused by non-existing channels.
  Unfortunately, there doesn't seem to be any way to detect if a symbol has these
  channels available.
  The error messages are therefore only logged as warnings.

* Using the "batch-" version of the order operations due to 5x higher rate limit
  allowance.

* There is no exchange API to support CancelAllOrders.
  This is therefore implemented by sending batch cancel requests for all
  known non-finished orders.

* There are a number of different "books" channels which can be used as source
  for MarketByPrice.
  Some of these require authentication **and** VIP membership.
  The gateway will by default choose the most detailed lowest latency feed.
  This is however not always correct.
  In particular, the gateway has no knowledge of the VIP membership status.
  These are the flags you can use to override default behaviour

  * :code:`--ws_books_depth`

    * 5 will use the :code:`books5` throttled **snapshot** feed
    * 50 will use the :code:`books50-l2-tbt` realtime incremental feed
    * 400 will use either the :code:`books-l2-tbt` realtime incremental feed or
      the :code:`books` throttled incremental feed.

  * :code:`--ws_books_use_public` is an opt-out flag because the gateway will
    by default use a realtime feed if an account has been configured.


References
----------


Common
~~~~~~

* :ref:`Using Conda <tutorial-conda>`
* :ref:`Using Flags <abseil-cpp>`
* :ref:`Gateway Flags <gateway-flags>`
* :ref:`Gateway Config <gateway-config>`


Exchange
~~~~~~~~

* `Website <https://www.okx.com/>`__
* `Documentation <https://www.okx.com/docs-v5/>`__
