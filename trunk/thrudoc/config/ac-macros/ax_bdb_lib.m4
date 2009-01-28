#AX_BDB_LIB
#
#Looks everywhere imaginable for berkeleydb
#
#If it finds somthing, sets
#
#HAVE_BERKELEYDB
#
#
AC_DEFUN([AX_BDB_LIB],
[
dnl
dnl  Look for Berkeley DB.
dnl 
AC_ARG_WITH(berkeleydb,
    [  --with-berkeleydb[=PATH] directory where BerkeleyDB exists],
    BERKELEY_DB_DIR=$with_berkeleydb,
    BERKELEY_DB_DIR=default)

AC_MSG_CHECKING(for BerkeleyDB location in $BERKELEY_DB_DIR)

if test "x$BERKELEY_DB_DIR" = "xno" ; then
    AC_MSG_RESULT(skipped)
else
    if test "x$BERKELEY_DB_DIR" = "xdefault" ; then
        for v in BerkeleyDB.4.2 BerkeleyDB.4.3 BerkeleyDB.4.4 BerkeleyDB.4.5 BerkeleyDB.4.6 BerkeleyDB.4.7; do
            for d in $prefix /opt /opt/local /usr/local /usr; do
                test -d "$d/$v" && BERKELEY_DB_DIR="$d/$v"
            done
        done
    fi

    if test "x$BERKELEY_DB_DIR" = "xdefault" ; then
        for v in . db47 db46 db45 db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do        
            for d in $prefix /opt /opt/local /usr/local /usr; do
                test -d "$d/include/$v" && BERKELEY_DB_DIR="$d"
            done
        done
    fi

    if test "x$BERKELEY_DB_DIR" = "xdefault" ; then

        for d in $prefix /opt /opt/local /usr/local /usr; do
            for v in db-4 db4 db3 db db40; do

                if test -f "$d/include/$v/db_cxx.h" ; then
                    echo "Found header in $d/include/$v "
                    test "x$d" != "x/usr" && BERKELEY_DB_LDFLAGS="-L$d/lib"
                    BERKELEY_DB_CPPFLAGS="-I$d/include/$v"
                    late_LIBS=$LIBS
		    # In RedHat 8, for instance, we have /usr/include/db4
		    # and libdb-4.0.a.  Debian has /usr/lib/libdb-4.1.a, for
		    # instance.  Look for the appropriate library.
                    if test $v = db4 -o $v = db40; then
		       	    save_CPPFLAGS="$CPPFLAGS"
			    save_LDFLAGS="$LDFLAGS"
			    CPPFLAGS="$CPPFLAGS $BERKELEY_DB_CPPFLAGS"
			    LDFLAGS="$LDFLAGS $BERKELEY_DB_LDFLAGS"
			    AC_SEARCH_LIBS(db_create, 
				[db-4 db4 db-4.7 db-4.6 db-4.5 db-4.4 db-4.3 db-4.2 db-4.1 db-4.0], 
				[BERKELEY_DB_LIB=$ac_cv_search_db_create])
			    CPPFLAGS="$save_CPPFLAGS"
			    LDFLAGS="$save_LDFLAGS"
		     else
			    BERKELEY_DB_LIB="-l$v"
                     fi
			    LIBS=$late_LIBS
                            AC_MSG_RESULT($d)
                            break
                fi
            done

	 test "x$BERKELEY_DB_LIB" != "x" && break
            if test -f "$d/include/db_cxx.h"; then
                if test "x$d" != "x/usr"; then
                    BERKELEY_DB_LDFLAGS="-L$d/lib64 -L$d/lib"
                    BERKELEY_DB_CPPFLAGS="-I$d/include"
                fi
                BERKELEY_DB_LIB="-ldb_cxx"
                AC_MSG_RESULT($d)
                break
            fi
	    done

        if test "x$BERKELEY_DB_LIB" = "x" ; then
            AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi

    elif test -f "$BERKELEY_DB_DIR/include/db_cxx.h" ;then
        case "$target_os" in
        solaris*)
            #If we are staticlly linking the BDB files, we do not want a
            #-R flag.  If .so's are present, assume we are dynamic linking
            if test -n "`ls $BERKELEY_DB_DIR/lib/*.so 2>/dev/null`"
            then
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib"
            else
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
            fi;;
        *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;
        esac
  
        BERKELEY_DB_CPPFLAGS="-I$BERKELEY_DB_DIR/include"
        BERKELEY_DB_LIB="-ldb_cxx"
        AC_MSG_RESULT($BERKELEY_DB_DIR)

    elif test -d "$BERKELEY_DB_DIR"; then

        BERKELEY_DB_CPPFLAGS="-I$BERKELEY_DB_DIR/include"
        for v in . db47 db46 db45 db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEY_DB_DIR/include/$v/db_cxx.h"; then
                BERKELEY_DB_CPPFLAGS="-I$BERKELEY_DB_DIR/include/$v"
			break
            fi
        done

        BERKELEY_DB_LIB="-ldb_cxx"
        for v in db-4.7 db4.7 db47 db-4.6 db4.6 db46 db-4.5 db4.5 db45 db-4.4 db4.4 db44; do
            if test -f "$BERKELEY_DB_DIR/lib/lib${v}_cxx.so"; then
                BERKELEY_DB_LIB="-l${v}_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib64/lib${v}_cxx.so"; then
                BERKELEY_DB_LIB="-l${v}_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib64"
			break
            fi
	    if test -f "$BERKELEY_DB_DIR/lib/lib${v}_cxx.dylib"; then
                BERKELEY_DB_LIB="-l${v}_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib64/lib${v}_cxx.dylib"; then
                BERKELEY_DB_LIB="-l$v"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib64"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib/$v/libdb_cxx.so"; then
                BERKELEY_DB_LIB="-ldb_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib/$v"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib/$v/libdb_cxx.dylib"; then
                BERKELEY_DB_LIB="-ldb_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib/$v"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib64/$v/libdb_cxx.so"; then
                BERKELEY_DB_LIB="-ldb_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib64/$v"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib64/$v/libdb_cxx.dylib"; then
                BERKELEY_DB_LIB="-ldb_cxx"
		BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib64/$v"
			break
            fi
        done

    
        AC_MSG_RESULT($BERKELEY_DB_DIR)

    elif test -d "$BERKELEY_DB_DIR"; then

        BERKELEY_DB_CPPFLAGS="-I$BERKELEY_DB_DIR/include"
        for v in . db47 db46 db45 db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEY_DB_DIR/include/$v/db.h"; then
                BERKELEY_DB_CPPFLAGS="-I$BERKELEY_DB_DIR/include/$v"
			break
            fi
        done

        BERKELEY_DB_LIB="-ldb_cxx"
        for v in db-4.7 db4.7 db47 db-4.6 db4.6 db46 db-4.5 db4.5 db45 db-4.4 db4.4 db44; do
            if test -f "$BERKELEY_DB_DIR/lib/lib${v}_cxx.so"; then
                BERKELEY_DB_LIB="-l${v}_cxx"
			break
            fi
            if test -f "$BERKELEY_DB_DIR/lib64/lib${v}_cxx.so"; then
                BERKELEY_DB_LIB="-l${v}_cxx"
			break
            fi
        done

        case "$target_os" in
        solaris*)
            #If we are staticlly linking the BDB files, we do not want a
            #-R flag.  If .so's are present, assume we are dynamic linking
            if test -n "`ls $BERKELEY_DB_DIR/lib/*.so 2>/dev/null`"
            then
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib"
            else
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
            fi;;
        *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;
        esac
  
        AC_MSG_RESULT($BERKELEY_DB_DIR)
    else
        AC_MSG_ERROR(Cannot find BerkeleyDB)
    fi
    AC_DEFINE(USE_BERKELEY_DB, 1, [Define if BerkeleyDB is available.])

    BERKELEY_DB_SAVE_LDFLAGS=$LDFLAGS
    BERKELEY_DB_SAVE_CPPFLAGS=$CPPFLAGS
    BERKELEY_DB_SAVE_LIBS=$LIBS
    LDFLAGS="$LDFLAGS $BERKELEY_DB_LDFLAGS"
    CPPFLAGS="$CPPFLAGS $BERKELEY_DB_CPPFLAGS"
    LIBS="$LIBS $BERKELEY_DB_LIB"

    AC_LANG_PUSH([C++])


    AC_MSG_CHECKING(Berkeley DB Version)
    
    AC_TRY_RUN(
       [
#include <db_cxx.h>
#include <stdio.h>
int main(void) 
{
    printf("%d.%d.%d ",DB_VERSION_MAJOR,DB_VERSION_MINOR,DB_VERSION_PATCH);
    if (DB_VERSION_MAJOR < 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 1))
        return 1;
    return 0;
}
       ],
       [AC_MSG_RESULT(OK) AC_DEFINE(HAVE_BERKELEYDB,1,Define if new Berkeley API)],
       [AC_MSG_RESULT(no); BERKELEY_DB_LDFLAGS=""; BERKELEY_DB_CPPFLAGS=""; BERKELEY_DB_LIB=""]
    )

    LDFLAGS=$BERKELEY_DB_SAVE_LDFLAGS
    CPPFLAGS=$BERKELEY_DB_SAVE_CPPFLAGS
    LIBS=$BERKELEY_DB_SAVE_LIBS

fi
AC_SUBST(BERKELEY_DB_LDFLAGS)
AC_SUBST(BERKELEY_DB_CPPFLAGS)
AC_SUBST(BERKELEY_DB_LIB)
])
