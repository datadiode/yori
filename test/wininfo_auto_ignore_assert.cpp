::size_t unused(void); /*
setlocal
pushd `ypath -f $PARENT$ "%~SCRIPTNAME%"`
set BASE=`ypath -f $BASE$ "%~SCRIPTNAME%"`
set FILE=`ypath -f $FILE$ "%~SCRIPTNAME%"`
builtin echo "<?xml version='1.0' encoding='UTF-8'?>" > %BASE%.xml
builtin echo "<testsuites>" >> %BASE%.xml
builtin echo "<testsuite name='%FILE%'>" >> %BASE%.xml
builtin echo "<testcase name='%BASE%' classname='Tests'>" >> %BASE%.xml
builtin echo "<system-out>" >> %BASE%.xml
cl.exe /nologo /FI stddef.h /Tp %FILE% >> %BASE%.xml
%BASE% &
sleep 500ms
wininfo -d -t "Microsoft Visual C++ Runtime Library" -p -b 5 >> %BASE%.xml
builtin echo "</system-out>" >> %BASE%.xml
builtin echo "</testcase>" >> %BASE%.xml
builtin echo "</testsuite>" >> %BASE%.xml
builtin echo "</testsuites>" >> %BASE%.xml
popd
endlocal
goto :eof
*/
#include <stdlib.h>
#include <assert.h>

int main(void)
{
	_set_error_mode(_OUT_TO_MSGBOX);
    assert(false);
	return 0;
}
