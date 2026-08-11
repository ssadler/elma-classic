
function build () {
  meson compile -C build
}

function dev () {                                                     
  cd build
  ./elma
  #valgrind --tool=callgrind node ts/test.js                          
}                                                                     
                                                                      
                                                                      
                                                                      
if [ -z "$1" ]; then                                                  
  echo "Commands:"                                                    
  echo                                                                
  cat $0 | sed -rne 's/^function ([^_][^ \(]+).*/  \1/p'              
  echo                                                                
else                                                                  
                                                                      
  cmd=$1           # Get the function name from argv                  
  shift            # Remove function name                             
  eval $cmd $@     # Call function and parse arguments                
fi                                                                    
                                                                      

