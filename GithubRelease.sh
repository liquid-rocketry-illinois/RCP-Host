cd $2

which gh > /dev/null
if [ $? -ne 0 ]; then
  echo "Github CLI is required for this auto-release."
  echo "Github CLI is required for this auto-release."
  exit 1
fi

read -p "Are all changes pushed? (Y/N): " confirm && [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]] || exit 1
git add . &> /dev/null && git diff --quiet && git diff --cached --quiet
if [ $? -ne 0 ]; then
  read -p "You have uncommitted changes. Still continue? (Y/N): " confirm && [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]] || exit 1
fi

cd $1

./RCP-Host-tests.exe
if [ $? -ne 0 ]; then
  echo "Some tests are failing!"
  read -p "Press key to continue... "
  exit 1
fi

mkdir -p releaseNotes
tagname=$(cat $2/VERSION)
echo "Using version $tagname"
echo $tagname > releaseNotes/tagname

echo "Complete Release Notes"
echo "# Release Notes" > releaseNotes/notes
notepad releaseNotes/notes

git tag $tagname -s -m "$tagname"
git push --tags

gh release create $tagname --notes-file releaseNotes/notes