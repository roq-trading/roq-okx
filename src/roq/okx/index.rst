.. _roq-okx:

.. |checkmark| unicode:: U+2713


roq-okx
=======

.. important::
   The account must be configured for the "net" position mode (futures and swaps).

.. tab:: Stable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/stable \
           roq-okx

.. tab:: Unstable

  .. code-block:: shell

     $ conda install \
           --channel https://roq-trading.com/conda/unstable \
           roq-okx


:code:`roq-okx`
---------------

.. code-block:: shell

   $ roq-okx [FLAGS]


Description
~~~~~~~~~~~

:code:`roq-okx` is a gateway


Supports
~~~~~~~~

.. grid::  2
  :gutter: 2

  .. grid-item-card::  Products

    .. list-table::
      :widths: auto

      * - Spot
        - |checkmark|
      * - Futures
        - |checkmark|
      * - Options
        - |checkmark|
      * - Combos
        -

  .. grid-item-card::  Market Data

    .. list-table::
      :widths: auto

      * - Reference Data
        - |checkmark|
      * - Market Status
        -
      * - Top of Book
        - |checkmark|
      * - Market by Price
        - |checkmark|
      * - Market by Order
        -
      * - Trade Summary
        - |checkmark|
      * - Statistics
        - |checkmark|

  .. grid-item-card::  Order Management

    .. list-table::
      :widths: auto

      * - Create
        - |checkmark|
      * - Modify
        - |checkmark|
      * - Cancel
        - |checkmark|
      * - Cancel All
        - |checkmark|
      * - Auto-Cancel
        -

  .. grid-item-card::  Account Management

    .. list-table::
      :widths: auto

      * - Positions
        - |checkmark|
      * - Funds
        - |checkmark|


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

.. tab:: Download

   .. include:: flags/download.rstinc

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

      $ $CONDA_PREFIX/share/roq-okx/flags/prod/flags.cfg

   .. include:: flags/prod/flags.cfg
     :code: ini

.. tab:: Test

   .. code-block:: shell

      $ $CONDA_PREFIX/share/roq-okx/flags/test/flags.cfg

   .. include:: flags/test/flags.cfg
     :code: ini


Configuration
~~~~~~~~~~~~~

.. code-block:: shell

   $ $CONDA_PREFIX/share/roq-okx/config.toml

.. important::

   The template will be replaced when the software is upgraded.
   Make a copy and modify to your needs.

.. include:: config.toml
   :code: toml


Market Data
~~~~~~~~~~~

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::ReferenceData`
      - MarketData
      - instruments
      -

    * - :cpp:class:`roq::MarketStatus`
      - MarketData
      - instruments
      -

    * - :cpp:class:`roq::TopOfBook`
      - MarketData
      - bbo-tbt
      -

    * - :cpp:class:`roq::MarketByPriceUpdate`
      - MarketData
      - books5, books, books50-l2-tbt, or books-l2-tbt
      -

    * - :cpp:class:`roq::MarketByOrderUpdate`
      -
      -
      -

    * - :cpp:class:`roq::TradeSummary`
      - MarketData
      - trades
      -

    * - :cpp:class:`roq::StatisticsUpdate`
      - MarketData
      - tickers, index-tickers, funding-rate
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::ReferenceData`
      -
      -
      -

    * - :cpp:class:`roq::MarketStatus`
      -
      -
      -

    * - :cpp:class:`roq::TopOfBook`
      -
      -
      -

    * - :cpp:class:`roq::MarketByPriceUpdate`
      -
      -
      -

    * - :cpp:class:`roq::MarketByOrderUpdate`
      -
      -
      -

    * - :cpp:class:`roq::TradeSummary`
      -
      -
      -

    * - :cpp:class:`roq::StatisticsUpdate`
      -
      -
      -


Statistics
^^^^^^^^^^

.. list-table::
  :header-rows: 1
  :widths: auto

  * - Type
    - Comments

  * - :cpp:class:`OPEN_PRICE`
    - (tickers) :code:`open24h`

  * - :cpp:class:`HIGHEST_TRADED_PRICE`
    - (tickers) :code:`high24h`

  * - :cpp:class:`LOWEST_TRADED_PRICE`
    - (tickers) :code:`low24h`

  * - :cpp:class:`TRADE_VOLUME`
    - (tickers) :code:`vol24h`

  * - :cpp:class:`FUNDING_RATE`
    - (funding-rate) :code:`fundingRate`

  * - :cpp:class:`FUNDING_RATE_PREDICTION`
    - (funding-rate) :code:`nextFundingRate`

  * - :cpp:class:`INDEX_VALUE`
    - (index-tickers) :code:`idxPx`


Order Management
~~~~~~~~~~~~~~~~

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderUpdate`
      - OrderEntry
      - orders
      -

    * - :cpp:class:`roq::TradeUpdate`
      -
      -
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderUpdate`
      - DropCopy
      - /api/v5/trade/orders-pending
      -

    * - :cpp:class:`roq::TradeUpdate`
      -
      -
      -

.. tab:: Request

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::CreateOrder`
      - OrderEntry
      - batch-orders
      -

    * - :cpp:class:`roq::ModifyOrder`
      - OrderEntry
      - batch-amend-orders
      -

    * - :cpp:class:`roq::CancelOrder`
      - OrderEntry
      - batch-cancel-orders
      -

    * - :cpp:class:`roq::CancelAllOrders`
      - OrderEntry
      - batch-cancel-orders
      - The exchange API does not support this atomically

.. tab:: Response

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::OrderAck`
      - OrderEntry
      - order, amend-order, cancel-order
      -

Mapping
^^^^^^^

.. list-table::
  :header-rows: 1
  :widths: auto

  * - OrderType
    - TimeInForce
    - ExecutionInstruction
    -
    - ordType
    - reduceOnly

  * - :cpp:class:`MARKET`
    -
    - 
    - →
    - :code:`market`
    - :code:`false`

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


Account Management
~~~~~~~~~~~~~~~~~~

.. tab:: Live

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::PositionUpdate`
      -
      -
      -

    * - :cpp:class:`roq::FundsUpdate`
      -
      -
      -

.. tab:: Download

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Event
      - Stream
      - Messages
      - Comments

    * - :cpp:class:`roq::PositionUpdate`
      -
      -
      -

    * - :cpp:class:`roq::FundsUpdate`
      - OrderEntry
      - /api/spot/v3/accounts
      -


Streams
~~~~~~~

.. tab:: DropCopy

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - REST
      - Primary purpose

        * download orders

        Each connection

        * supports a single account

.. tab:: OrderEntry

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - WebSocket
      - Primary purpose

        * support order management

        Each connection

        * supports a single account

.. tab:: MarketData

  .. list-table::
    :header-rows: 1
    :widths: auto

    * - Type
      - Comments

    * - WebSocket
      - Primary purpose

        * live market data
        * reference data and markets (from the :code:`instruments` channel)

        Each connection

        * supports a slice of the symbols

        The first stream is used to

        * subscribe the :code:`instruments` channel


Constraints
~~~~~~~~~~~

* Only VIP members can access the L2 tick-by-tick market data feed.


Comments
~~~~~~~~

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

OKX
~~~

* `Website <https://www.okx.com/>`__
* `Support <mailto:support@okex.com>`__
* `Documentation <https://www.okx.com/docs-v5/en/>`__
