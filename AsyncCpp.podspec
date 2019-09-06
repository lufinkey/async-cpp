#
# Be sure to run `pod lib lint AsyncCpp.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see https://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
	s.name             = 'AsyncCpp'
	s.version          = '0.1.0'
	s.summary          = 'A small async library for C++.'

# This description is used to generate tags and improve search results.
#   * Think: What does it do? Why did you write it? What is the focus?
#   * Try to keep it short, snappy and to the point.
#   * Write the description between the DESC delimiters below.
#   * Finally, don't worry about the indent, CocoaPods strips it!

	s.description      = <<-DESC
	A small async library for C++.
							DESC

	s.homepage         = 'https://github.com/lufinkey/async-cpp'
	# s.screenshots     = 'www.example.com/screenshots_1', 'www.example.com/screenshots_2'
	# s.license          = { :type => 'MIT', :file => 'LICENSE' }
	s.author           = { 'Luis Finke' => 'luisfinke@gmail.com' }
	s.source           = { :git => 'https://github.com/lufinkey/async-cpp.git', :tag => s.version.to_s }
	# s.social_media_url = 'https://twitter.com/<TWITTER_USERNAME>'

	s.ios.deployment_target = '9.0'
	s.osx.deployment_target = '10.14'

	s.source_files = 'src/fgl/**/*'
  
	# s.resource_bundles = {
	#   'AsyncCpp' => ['AsyncCpp/Assets/*.png']
	# }

	s.public_header_files = 'src/fgl/**/*.hpp'
	s.header_mappings_dir = 'src/fgl'
	s.xcconfig = {
		'HEADER_SEARCH_PATHS' => '"${PODS_ROOT}/AsyncCpp/src"',
		'CLANG_CXX_LANGUAGE_STANDARD' => 'gnu++17'
	}
	# s.frameworks = 'UIKit', 'MapKit'
	s.dependency 'DataCpp' # git@github.com:lufinkey/data-cpp.git
end
