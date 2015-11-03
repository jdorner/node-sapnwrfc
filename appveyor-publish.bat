IF %appveyor_repo_branch%==master (
  git config user.email "joachim.dorner@gmail.com"
  git config user.name "jdorner"
  git pull origin %APPVEYOR_REPO_BRANCH%
  git add -f compiled/%NODE_VERSION_SHORT%/win32/x64/sapnwrfc.node
  git commit -m "[ci skip] Add Windows binding for Node.js %NODE_VERSION_SHORT%"
  git push
)
