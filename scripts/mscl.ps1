function Invoke-CmdScript {
    param(
        [String] $scriptName
    )
    $cmdLine = """$scriptName"" $args & set"
    & $Env:SystemRoot\system32\cmd.exe /c $cmdLine |
    select-string '^([^=]*)=(.*)$' | foreach-object {
        $varName = $_.Matches[0].Groups[1].Value
        $varValue = $_.Matches[0].Groups[2].Value
        set-item Env:$varName $varValue
    }
}
Invoke-CmdScript "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
cl