#' Title
#'
#' @param name A string name
#'
#' @return A hello type message
#' @export
#'
#' @examples hello("Alice")
hello <- function(name)
{
  message <- paste0("Hello ", name, "!")
  return(message)
}
