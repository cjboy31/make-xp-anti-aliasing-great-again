function New-NativeAALayerManifest([string]$LibraryName) {
 if($LibraryName -notmatch '^native-aa-[a-f0-9]{12}\.dll$'){throw 'Invalid Native AA renderer library name'}
 # The Windows loader resolves a relative library path only with native path
 # separators. A Unix-style ./ path failed LoadLibraryExW with error 87.
 return [ordered]@{file_format_version='1.2.0';layer=[ordered]@{
  name='VK_LAYER_NATIVE_AA_live_test';type='GLOBAL';library_path=('.\'+$LibraryName)
  api_version='1.3.0';implementation_version='2';description='Independent native scene AA; exact selected simulator only'
  functions=@{vkNegotiateLoaderLayerInterfaceVersion='aaNegotiate'};disable_environment=@{NATIVE_AA_DISABLE='1'}
 }}
}
