''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' VBScript to run some PCRE2 tests against SRELL.RegExp COM Wrapper
' Refer to https://github.com/PCRE2Project/pcre2/blob/master/testdata
' for suitable input files, esp. testinput1
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
Option Explicit

Dim Verbose
Verbose = WScript.Arguments.Named("verbose")

Dim YoriStrCmp
YoriStrCmp = WScript.Arguments.Named("ystrcmp")

Dim RegExpClass
RegExpClass = Replace("SRELL.RegExp", "SRELL", "VBScript", 1, YoriStrCmp <> "" Or WScript.Arguments.Named("builtin-regexp"))

Dim re
Set re = CreateObject(RegExpClass)

Dim fs
Set fs = CreateObject("Scripting.FileSystemObject")

Dim sh
Set sh = CreateObject("WScript.Shell")

Sub RunTest(testinput)
	Dim stm, line, j, c, identified, compilable, uncompilable, ignored, polarity, failed, passed, messed, matched, singleline
	Set stm = fs.OpenTextFile(testinput)
	While Not stm.AtEndOfStream
		line = stm.ReadLine
		If InStrRev(line, "/", 1) = 1 Then
			Do
				j = InStrRev(line, "/")
				If j > 1 Then
					' guess if this is the end of the regexp
					If Mid(line, j - 1, 1) <> "\" Then
						c = Mid(line, j + 1, 1)
						If Len(c) = 0 Or UCase(c) <> LCase(c) Then Exit Do
					End If
				End If
				line = line & stm.ReadLine
			Loop Until stm.AtEndOfStream
			If Verbose Then WScript.Echo line
			re.pattern = Mid(line, 2, j - 2)
			re.IgnoreCase = InStrRev(line, "i") > j
			re.MultiLine = InStrRev(line, "m") > j
			On Error Resume Next
			re.SingleLine = InStrRev(line, "s") > j
			singleline = Err.Number = 0
			On Error GoTo 0
			identified = identified + 1
			If InStrRev(line, "x") > j Or Not singleline And InStrRev(line, "s") > j Or line = "/(a+)*b/" Then
				' Neither srellcom nor VBScript.RegExp support the x modifier
				' VBScript.RegExp does not support the s modifier
				' The /(a+)*b/ case is computationally too demanding
				ignored = ignored + 1
				polarity = 0
			Else
				On Error Resume Next
				re.Test ""
				If Err.Number = 0 Then
					polarity = 1
					compilable = compilable + 1
				Else
					polarity = 0
					If Verbose Then WScript.Echo Err.Source & "-" & Err.Number & ": " & Err.Description
					uncompilable = uncompilable + 1
				End If
				On Error GoTo 0
			End If
		ElseIf InStrRev(line, "\=", 2) = 1 Then
			polarity = -polarity
		ElseIf InStrRev(line, "#", 1) = 0 Then
			line = Trim(line)
			line = Replace(line, "\r", vbCr)
			line = Replace(line, "\n", vbLf)
			line = Replace(line, "\t", vbTab)
			line = Replace(line, "\f", Chr(12))
			line = Replace(line, "\\", "\")
			line = Replace(line, "\'", "'")
			line = Replace(line, "\""", """")
			If polarity <> 0 And line <> "" Then
				If Verbose Then WScript.Echo line
				On Error Resume Next
				If YoriStrCmp <> "" Then
					matched = sh.Run(YoriStrCmp & " " & Replace("-i", "-i", "", 1, Abs(Not re.IgnoreCase)) & " -- " & line & "=~" & re.pattern, 0, True)
				Else
					matched = Abs(Not re.Test(line))
				End If
				If Err.Number <> 0 Or matched < 0 Then
					messed = messed + 1
					If Verbose Then WScript.Echo "*** MESSED ***"
				ElseIf polarity > 0 Xor matched = 0 Then
					failed = failed + 1
					If Verbose Then WScript.Echo "*** FAILED ***"
				Else
					passed = passed + 1
				End If
				On Error GoTo 0
			End If
		End If
	WEnd
	WScript.Echo "<testcase name='" & fs.GetFileName(testinput) & "' classname='Tests'>"
	WScript.Echo "<system-out>"
	WScript.Echo "identified expressions = " & identified
	WScript.Echo "compilable expressions = " & compilable
	WScript.Echo "uncompilable expressions = " & uncompilable
	WScript.Echo "ignored expressions = " & ignored
	WScript.Echo "passed recognitions = " & passed
	WScript.Echo "failed recognitions = " & failed
	WScript.Echo "messed recognitions = " & messed
	WScript.Echo "</system-out>"
	WScript.Echo "</testcase>"
End Sub

WScript.Echo "<?xml version='1.0' encoding='UTF-8'?>"
WScript.Echo "<testsuites>"
WScript.Echo "<testsuite name='pretest.vbs'>"

If Verbose Then WScript.Echo "RegExpClass = " & RegExpClass

Dim testinput
For Each testinput In WScript.Arguments.Unnamed
	RunTest testinput	
Next

WScript.Echo "</testsuite>"
WScript.Echo "</testsuites>"
