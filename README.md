Plume
=====

A high performace HTTP cache-proxy server which support multiple thread and plugin programming.
Also Plume is a server side framework and now just support linux.
We can develop specific plugin to accomplish specific business.
Plume contians a simple echo plugin as default. We can use it like this:

    main {
        logpath /usr/local/plume/logs
        work_thread_num 1
        maxfd 1024
        load_plugin /usr/local/plume/plugin/libplm_echo.so echo_plugin
        
        echo {
            $REPLY_STR=Hello From Plume
            echo_port 3338
            echo_str $REPLY_STR
        }
        
        # reply client with what we received
        # echo {
        #    echo_port 3338
        # }
    }

For example we have a plugin named 'http' to serve as web server. We can edit the conf file.

    main {
        ......
        load_plugin /your_path/libhttp.so http_plugin
        http {
            http_port 80
            ......
        }
        ......
    }

We can develop plugin with specific instruction, for example echo plugin has instructions like:

    echo_port -- set echo server listen port number
    echo_str -- set reply string text

Plume configure file support variable. Every variable begins with $. Like:

    $REPLY_STR=Hello From Plume
    echo_str $REPLY_STR


How to build:

    $ cd plume
    $ autoreconf -if
    $ ./configure
    $ make
    $ sudo make install
    
build the echo plugin:

    $ cd src/plugin/echo
    $ make 
    $ sudo cp libplm_echo.so /your_path/plugin

As default plume will be installed in /usr/local/plume. We can start it:

    $ sudo /usr/local/plume/bin/plume

Also we can change the default install path by pass --prefix=xxx to ./configure.
As default, install path contains 4 dirs.

    logs/ -- all log files and pid file
    conf/ -- plume.conf, the configure file
    bin/ -- the executeable file
    plugin/ -- plugin file path

How to stop:

    $ sudo /your_path/plume -s quit

How to reconfigure on fly:

    $ sudo /your_path/plume -s reconfigure

As default plume will be run with 2 process, one master process and one worker process.
The master process monitor worker process and do other signal operations.
The worker process run the business.

Worker process is designed with multiple threads architecture. 
The number of threads could be set by work_thread_num instruction in main.

How to run as single process mode:

    $ sudo /your_path/plume -S

How to run as non-daemon process:

    $ sudo /your_path/plume -N

As my intention at the beginning is develop a web proxy-cache server app, 
which is high performance and scalability.
The feature is support multiple thread, module programming etc.

Other instruction support now:

    work_thread_cpu_affinity -- set thread cpu affinity
    for example: 
    work_thread_num 2
    work_thread_cpu_affinity 01 10

Thanks for testing and bug report(yykxx@hotmail.com).
