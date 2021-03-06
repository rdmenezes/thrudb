#!/usr/bin/perl

use strict;
use warnings;

use lib './gen-perl';

use Thrift;
use Thrift::BinaryProtocol;
use Thrift::Socket;
use Thrift::FramedTransport;

use Data::Dumper;
use Time::HiRes qw(gettimeofday);
use Thruqueue::Thruqueue;

my $socket    = new Thrift::Socket('localhost',9093);
my $transport = new Thrift::FramedTransport($socket);
my $protocol  = new Thrift::BinaryProtocol($transport);
my $client    = new Thruqueue::ThruqueueClient($protocol);

my $count = shift || 100;

my $t0 = gettimeofday();

$transport->open();

warn "connected";

my $queue_name = "test_queue";

eval{

    $client->createQueue($queue_name);
    warn "created";

    foreach my $i (1..$count){

        #$client->sendMessage($queue_name,"message $i"x800);
    }

    $client->queueLength($queue_name);

    foreach my $i (1..$count){
        $client->readMessage($queue_name,10);
        $client->deleteMessage($queue_name);
    }

    my $t1 = gettimeofday();

    print "Added $count docs in ".sprintf("%0.2f",($t1-$t0))."Secs\n";

}; if($@){
    warn(Dumper($@));
}
