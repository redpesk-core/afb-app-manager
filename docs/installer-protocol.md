# Installer protocol

This document describes the protocol used by the daemon **afmpkg-installer**.

The two main clients of the protocol are:

- the redpesk plugin for RPM that triggers installation and removal of packages
- the program **afmpkg-status** that gets the status of a previous installation

When **dnf** is called, it resolves dependencies and install all the packages
required. This leads to installing possibly more than one package.
The daemon **afmpkg-installer** handles it with the concept of transaction.

So installation can take places

The protocol specifies that only one request is served by **afmpkg-installer**.
So the client connects, put an order, wait the answer and disconnect.

So the connection can be summarized by the below synopsis:

```
REQUEST  (C->S)
REPLY    (S->C)
```

The protocol is made of lines of text.

At any time, on any line, if **afmpkg-installer** detects a protocol error, it sends
an error reply and closes the connection.

There is two kinds of requests:

- the package request that request to install or remove packages
- the status request that gets the final status of a transaction

```
REQUEST ::= PACKAGE-REQUEST | STATUS-REQUEST
```

## PACKAGE-REQUEST

A PACKAGE-REQUEST is made of several lines, starting with a beginning line and stopping
with an ending line.

```
PACKAGE-REQUEST ::=  BEGIN-LINE [BODY-LINE]... END-LINE

BEGIN-LINE ::= 'BEGIN' SP OPERATION EOL
END-LINE   ::= 'END' SP OPERATION EOL
OPERATION  ::= 'ADD' | 'REMOVE'
```

It is an error if the OPERATION given at end line doesn't match the operation
given at begin line.

The operation ADD is used for installing packages, the operation REMOVE
for removing it.

The lines of the body can be send in unspecified order.
Except lines for FILE, lines can occur only once.

Body lines are of 2 main kinds:
- lines related to the package
- lines related to the transaction

```
BODY-LINE ::= PACK-LINE | TRANS-LINE
```

The lines related to the package are giving the name of the package and
the paths of the installed files.

```
PACK-LINE ::= PACKAGE-LINE | ROOT-LINE | FILE-LINE | REDPAKID-LINE

PACKAGE-LINE  ::= 'PACKAGE' SP NAME EOL
ROOT-LINE     ::= 'ROOT' SP PATH EOL
FILE-LINE     ::= 'FILE' SP PATH EOL
REDPAKID-LINE ::= 'REDPAKID' SP ID EOL
```

The lines related to the transaction are giving the identifier of
the transaction, the index of the package within the given count.

```
TRANS-LINE    ::= COUNT-LINE | INDEX-LINE | TRANSID-LINE

COUNT-LINE    ::= 'COUNT' SP NUMBER EOL
INDEX-LINE    ::= 'INDEX' SP NUMBER EOL
TRANSID-LINE  ::= 'TRANSID' SP ID EOL
```


## STATUS-REQUEST

A STATUS-REQUEST is made of exctly one status line.

```
STATUS-REQUEST ::= 'STATUS' SP TRANSID EOL
```

## REPLY

The reply is made of a single line of text

```
REPLY       ::= OK-REPLY | ERROR-REPLY
OK-REPLY    ::= 'OK' [SP ANY-TEXT] EOL
ERROR-REPLY ::= 'ERROR' [SP ANY-TEXT] EOL
```

For ok reply to the the STATUS request, the text is made of 3 numbers:

1. the count of package of the transaction
2. the count of packages of the transaction processed successfully
3. the count of packages of the transaction processed with error

