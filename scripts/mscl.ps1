function Invoke-CmdScript {
    param(
        [String] $scriptName
    )

    # Constructing the command line
    $cmdLine = """$scriptName"" $args & set"

    # Invoking the command script using cmd.exe and capturing the output
    & $Env:SystemRoot\system32\cmd.exe /c $cmdLine |
    select-string '^([^=]*)=(.*)$' | foreach-object {
        $varName = $_.Matches[0].Groups[1].Value
        $varValue = $_.Matches[0].Groups[2].Value

        # Setting the environment variable using set-item
        set-item Env:$varName $varValue
    }
}

# Invoking the cmd script "vcvars64.bat" from Visual Studio 2019
Invoke-CmdScript "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

# Executing the 'cl' command
cl
