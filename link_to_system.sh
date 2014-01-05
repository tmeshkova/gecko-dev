#!/bin/sh

DISTBINDIR=$1
if [ "$DISTBINDIR" = "" ]; then
  echo "DISTBINDIR not defined, please provide from object dir"
  exit
fi

TOPREFIX=$2
if [ "$TOPREFIX" = "" ]; then
  echo "TOPREFIX not defined, please provide prefix for installation"
  exit
fi


GREVERSION=`cat $DISTBINDIR/config/autoconf.mk | grep GRE_MILESTONE | sed 's/GRE_MILESTONE = //g'`

PREFIX=$TOPREFIX

DIST_NAME=xulrunner-$GREVERSION
DIST_DEV_NAME=xulrunner-devel-$GREVERSION

prepare()
{
    str=$1;
    strto=$2;
    strdest=$3;
    fname="${str##*/}";
    mkdir -p $PREFIX/$strto;
    rm $PREFIX/$strto/$strdest;
    ln -s $(pwd)/$str $PREFIX/$strto/$strdest;
}


prepare $DISTBINDIR/dist/idl share/idl $DIST_NAME
prepare $DISTBINDIR/dist/bin lib $DIST_NAME
prepare $DISTBINDIR/dist/include include $DIST_NAME
prepare $DISTBINDIR/xulrunner/installer/libxul-embedding.pc lib/pkgconfig libxul-embedding.pc
prepare $DISTBINDIR/xulrunner/installer/libxul.pc lib/pkgconfig libxul.pc
prepare $DISTBINDIR/xulrunner/installer/mozilla-nspr.pc lib/pkgconfig mozilla-nspr.pc
prepare $DISTBINDIR/xulrunner/installer/mozilla-js.pc lib/pkgconfig mozilla-js.pc
prepare $DISTBINDIR/xulrunner/installer/mozilla-nss.pc lib/pkgconfig mozilla-nss.pc
prepare $DISTBINDIR/xulrunner/installer/mozilla-plugin.pc lib/pkgconfig mozilla-plugin.pc
prepare $DISTBINDIR/dist/idl lib/$DIST_DEV_NAME idl
prepare $DISTBINDIR/dist/bin lib/$DIST_DEV_NAME bin
prepare $DISTBINDIR/dist/include lib/$DIST_DEV_NAME include
prepare $DISTBINDIR/dist/sdk/lib lib/$DIST_DEV_NAME lib
prepare $DISTBINDIR/dist/sdk/lib lib/$DIST_DEV_NAME/sdk lib
mkdir -p $PREFIX/lib/$DIST_DEV_NAME/sdk/bin
rm -rf $PREFIX/lib/$DIST_DEV_NAME/sdk/bin
cp -rfL $(pwd)/$DISTBINDIR/dist/sdk/bin $PREFIX/lib/$DIST_DEV_NAME/sdk/bin


